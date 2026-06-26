#include "efi.h"
#include "flash.h"
#include "gop.h"
#include "int.h"
#include "str.h"
#include "debug.h"
#include "pci.h"
#include "xhci.h"
#include "kmalloc.h"
#include "gdt.h"
#include "idt.h"
#include "Context.h"
#include "task.h"
#include "ata.h"
#include "gpt.h"
#include "fat32.h"
#include "mouse.h"
#include "keyboard.h"
#include "ohci.h"
#include "hda.h"

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_LARGE    (1ULL << 7)
#define PAGE_HUGE     (1ULL << 7)

#define POOL_PHYS_START 0x04000000ULL

u32* GopOut=NULL;
u32* GopBack=NULL;

EFI_BOOT_SERVICES *bs=NULL;
EFI_GOP_MODE GopMode={0};
EFI_GOP_MODE_INFO GopInfo={0};
EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop=NULL;

EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
RegContext MainGlobalContext={0};

EFI_HANDLE ImageBase;

AllocPool KernelPool = { .Head=NULL};

#pragma clang optimize off                                                      
__attribute__((optnone))
void SIMDInit(void) {
    unsigned long long cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9) | (1 << 10) | (1 << 18);
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
    MemFullFlash();
    unsigned long long xcr0 = 0;
    __asm__ volatile ("xgetbv" : "=A"(xcr0) : "c"(0));
    MemFullFlash();
    xcr0 |= (1 << 1) | (1 << 2);
    __asm__ volatile ("xsetbv" : : "c"(0), "A"(xcr0));                          
    MemFullFlash();                                                                        
}                                                                                               
void FPUInit(void){
    u64 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~((u64)0xE);
    cr0 |= 0x22;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}
#pragma clang optimize on

u64* BuildDynamicPageTable(u64 MaxPhysMem, EFI_BOOT_SERVICES* bs)
{
    const u64 GB = 1024ULL * 1024 * 1024;
    u64 bytes_per_pdpt = 512ULL * GB;
    u64 pdpt_count = (MaxPhysMem + bytes_per_pdpt - 1) / bytes_per_pdpt;
    if (pdpt_count == 0) pdpt_count = 1;
    if (pdpt_count > 512) pdpt_count = 512;

    u64 total_pages_needed = 1 + pdpt_count;

    EFI_PHYSICAL_ADDRESS allocated_buffer = 0;
    EFI_STATUS status = bs->AllocatePages(
        AllocateAnyPages,
        EfiRuntimeServicesData,
        total_pages_needed,
        &allocated_buffer
    );

    if (EFI_ERROR(status) || allocated_buffer == 0) {
        DebugStr("[CRITICAL] Failed to allocate page table memory!\r\n");
        while(1);
    }

    u8* BasePtr = (u8*)allocated_buffer;
    u64* pml4 = (u64*)BasePtr;
    MemSet64(pml4, 0, 512);
    
    for (u64 i = 1; i < pdpt_count; i++) {
        u64* pdpt = (u64*)(BasePtr + 4096 + i * 4096);
        MemSet64(pdpt, 0, 512);
        pml4[i] = ((u64)pdpt) | PAGE_PRESENT | PAGE_WRITABLE;
    }

    u64 table_start = allocated_buffer;
    u64 table_end   = allocated_buffer + total_pages_needed * 4096;
    u64 pt_gb_start = (table_start / GB) * GB;

    for (u64 phys = pt_gb_start; phys < table_end; phys += GB) {
        u32 pml4_idx = (phys >> 39) & 0x1FF;
        u32 pdpt_idx = (phys >> 30) & 0x1FF;
        if (pml4_idx >= pdpt_count) continue;
        u64* pdpt = (u64*)(pml4[pml4_idx] & 0xFFFFFFFFFFFFF000ULL);
        pdpt[pdpt_idx] = phys | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
    }

    u64 mapping_limit = pdpt_count * bytes_per_pdpt;
    u64 effective_max = (MaxPhysMem < mapping_limit) ? MaxPhysMem : mapping_limit;

    for (u64 phys = 1; phys < effective_max; phys += GB) {
        u32 pml4_idx = (phys >> 39) & 0x1FF;
        u32 pdpt_idx = (phys >> 30) & 0x1FF;
        if (pml4_idx >= pdpt_count) break;
        u64* pdpt = (u64*)(pml4[pml4_idx] & 0xFFFFFFFFFFFFF000ULL);
        pdpt[pdpt_idx] = phys | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
    }

    return pml4;
}

EFI_STATUS ExitBootServices_Safe(EFI_HANDLE ImageHandle) {
    EFI_STATUS status;
    UINTN MapKey;
    UINTN MemoryMapSize = 1024 * 8;
    u8 MemoryMapBuffer[1024 * 8];
    UINTN DescriptorSize;
    u32 DescriptorVersion;
    while (1) {
        MemoryMapSize = sizeof(MemoryMapBuffer);
        status = bs->GetMemoryMap(&MemoryMapSize, (void*)MemoryMapBuffer, &MapKey, &DescriptorSize, &DescriptorVersion);
        if (EFI_ERROR(status)) return status;
        status = bs->ExitBootServices(ImageHandle, MapKey);
        if (status == EFI_SUCCESS) break;
    }
    return status;
}

u64 GetXhciMmioBase() {
    for (u16 bus = 0; bus < 256; bus++) {
        for (u8 dev = 0; dev < 32; dev++) {
            for (u8 func = 0; func < 8; func++) {
                u32 id = PCIReadDWORD(bus, dev, func, 0x00);
                if (id == 0xFFFFFFFF) continue;
                u32 classRev = PCIReadDWORD(bus, dev, func, 0x08);
                u8 class = (classRev >> 24) & 0xFF;
                u8 subclass = (classRev >> 16) & 0xFF;
                u8 progIf = (classRev >> 8) & 0xFF;
                if (class == 0x0C && subclass == 0x03 && progIf == 0x30) {
                    PCIEnableDevice(bus, dev, func);
                    return PCIGetBARAddress(bus, dev, func, 0);
                }
            }
        }
    }
    return 0;
}

void kernel_main() {
    DebugStr("Jumped To KernelMain.\n");
    GopMode.FrameBufferSize = (size_t)GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution * sizeof(u32);
    XhciController* Xhci = XhciInit(GetXhciMmioBase());
    if (Xhci) XhciScanPorts(Xhci);
    DebugStr("XhciScanPorts Finsh.\n");
    ATAInit();
    if (GPTDetect(0, 0)) Fat32Init(0, 0);
    if (GPTDetect(0, 1)) Fat32Init(0, 1);
    if (GPTDetect(1, 0)) Fat32Init(1, 0);
    if (GPTDetect(1, 1)) Fat32Init(1, 1);
    while(1){
        TaskPoll();
        GOPClearAlpha(0x80000000);
        GOPClearAlpha(0x80FF0000);
        GOPFlash();
    }
}

void InitPool(u64 PoolSize){
    AllocBlock *block = (AllocBlock*)POOL_PHYS_START;
    block->size=PoolSize - sizeof(AllocBlock);
    block->is_free=1;
    block->next = NULL;
    KernelPool.Head = block;
}

EFI_STATUS EFIAPI Cefi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *sys) {
    bs=sys->BootServices;
    SIMDInit();
    FPUInit();
    DebugInit();
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    
    EFI_STATUS status = sys->BootServices->HandleProtocol(
        image, 
        (EFI_GUID *)&gEfiLoadedImageProtocolGuid, 
        (void **)&LoadedImage
    );

    if (status == EFI_SUCCESS && LoadedImage != NULL) {
        u64 TrueImageBase = (u64)LoadedImage->ImageBase;
        u64 TrueImageSize = LoadedImage->ImageSize;
        
        DebugStr("\r\n[NeuroSamaOS] UEFI LOADED IMAGE BASE: ");
        DebugU64(TrueImageBase);
        DebugStr("\r\n[NeuroSamaOS] IMAGE SIZE: ");
        DebugU64(TrueImageSize);
        
    } else {
        DebugStr("\r\n[ERROR] HandleProtocol failed to fetch ImageBase! Code: ");
        DebugU64(status);
    }

    bs->LocateProtocol(&GopGuid, NULL, (void **)&Gop);
    Gop->SetMode(Gop,0);
    MemCopy(&GopMode,Gop->Mode,sizeof(EFI_GOP_MODE));
    MemCopy(&GopInfo,Gop->Mode->Info,sizeof(EFI_GOP_MODE_INFO));
    GopOut=(u32*)(u64)GopMode.FrameBufferBase;

    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemMap = NULL;
    UINTN MapKey, DescriptorSize;
    u32 DescriptorVersion;
    bs->GetMemoryMap(&MemoryMapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    MemoryMapSize += 2 * DescriptorSize;
    bs->AllocatePool(EfiLoaderData, MemoryMapSize, (void**)&MemMap);
    bs->GetMemoryMap(&MemoryMapSize, (void*)MemMap, &MapKey, &DescriptorSize, &DescriptorVersion);

    UINTN EntryCount = MemoryMapSize / DescriptorSize;
    u64 MaxPhysicalAddress = 0;
    for (UINTN i = 0; i < EntryCount; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*)((u8*)MemMap + (i * DescriptorSize));
        u64 EndAddress = desc->PhysicalStart + (desc->NumberOfPages * 4096);
        if (EndAddress > MaxPhysicalAddress) MaxPhysicalAddress = EndAddress;
    }
    DebugStr("MaxPhysicalAddress: ");DebugU64(MaxPhysicalAddress);DebugChar('\n');
    
    u64 *pml4=BuildDynamicPageTable(MaxPhysicalAddress+10, bs);

    ExitBootServices_Safe(image);
    asm volatile("cli\n\t");

    MemFullFlash();
    asm volatile("mov %0, %%cr3" :: "r"(pml4) : "memory");

    u64 PoolSize = 0x40000000;
    if (POOL_PHYS_START + PoolSize > MaxPhysicalAddress) {
        PoolSize = MaxPhysicalAddress - POOL_PHYS_START;
    }
    InitPool(PoolSize);

    u8* KernelStack = AlignedAlloc(&KernelPool, 64*1024*1024,4096);
    GopBack = (u32*)(u64)AlignedAlloc(&KernelPool, GopMode.FrameBufferSize, 64);

    LoadGDT();
    InitInterruptSystem();

    void (*entry_point)(void) = kernel_main;

    u64 StackTop = (u64)KernelStack + 64*1024*1024;
    u64 StackBottom = (u64)KernelStack;
    asm volatile(
        "movq %0, %%rsp\n\t"
        "movq %1, %%rbp\n\t"
        "jmp *%2\n\t"
        :
        : "r"(StackTop), "r"(StackBottom), "r"(entry_point)
        : "memory"
    );
    return 0;
}
