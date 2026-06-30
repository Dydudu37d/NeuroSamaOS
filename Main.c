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
#include "gm206.h"
#include "uhci.h"
#include "UhciMouse.h"
#include "UhciKeyboard.h"

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_LARGE    (1ULL << 7)
#define PAGE_HUGE     (1ULL << 7)

#define POOL_PHYS_START 0x04000000ULL

void* GopOut=NULL;
HDR_PIXEL* GopBack=NULL;

EFI_BOOT_SERVICES *bs=NULL;
EFI_GOP_MODE GopMode={0};
EFI_GOP_MODE_INFO GopInfo={0};
EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop=NULL;

EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
RegContext MainGlobalContext={0};

#define PAGE_SIZE_4KB  0x1000ULL
#define PAGE_SIZE_2MB  0x200000ULL
#define PAGE_SIZE_1GB  0x40000000ULL
#define PAGE_SIZE_512GB 0x8000000000ULL
#define PAGE_SIZE_256TB 0xE80000000000ULL

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
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~((u64)0xE);
    cr0 |= 0x22;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}
#pragma clang optimize on

_Bool IsLa57Supported() {
    unsigned int eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
    return (ecx & (1 << 16)) != 0;
}

u64* BuildDynamicPageTable(u64 MaxPhysMem, u64 PhysMem, EFI_BOOT_SERVICES* bs) {
    EFI_PHYSICAL_ADDRESS alloc_addr;
    int use_l5 = IsLa57Supported();

    alloc_addr = 0xFFFFFFFF;
    alloc_addr = 0xFFFFFFFF;
    if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) {
        DebugStr("AllocatePages failed!\n");
        return 0;
    }
    u64* root_table = (u64*)alloc_addr;
    for (int i = 0; i < 512; i++) root_table[i] = 0;

    u64* pml4 = NULL;
    if (use_l5) {
        alloc_addr = 0xFFFFFFFF;
        if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
        pml4 = (u64*)alloc_addr;
        for (int i = 0; i < 512; i++) pml4[i] = 0;
        root_table[0] = ((u64)pml4 & ~0xFFFULL) | 0x003; 
    } else {
        pml4 = root_table; 
    }

    alloc_addr = 0xFFFFFFFF;
    if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
    u64* pdpt_0 = (u64*)alloc_addr;
    for (int i = 0; i < 512; i++) pdpt_0[i] = 0;
    pml4[0] = ((u64)pdpt_0 & ~0xFFFULL) | 0x003; 

    alloc_addr = 0xFFFFFFFF;
    if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
    u64* pd = (u64*)alloc_addr;
    for (int i = 0; i < 512; i++) pd[i] = 0;
    pdpt_0[0] = ((u64)pd & ~0xFFFULL) | 0x003; 

    alloc_addr = 0xFFFFFFFF;
    if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
    u64* pt = (u64*)alloc_addr;
    for (int i = 0; i < 512; i++) pt[i] = 0;
    pd[0] = ((u64)pt & ~0xFFFULL) | 0x003; 

    for (u64 i = 1; i < 512; i++) {
        u64 phys = i * 4096;
        if (phys >= MaxPhysMem) break;
        pt[i] = (phys >= 0xC8000000 && phys < 0xD0000000) ? (phys | 0x013) : (phys | 0x003);
    }
    
    for (u64 i = 1; i < 512; i++) {
        u64 phys = i * 2097152;
        if (phys >= MaxPhysMem) break;
        pd[i] = (phys >= 0xC8000000 && phys < 0xD0000000) ? (phys | 0x193) : (phys | 0x083);
    }

    u64 max_1gb_pages = (MaxPhysMem + 1073741824ULL - 1) / 1073741824ULL;
    if (max_1gb_pages > 512) max_1gb_pages = 512;

    for (u64 i = 0; i <= max_1gb_pages; i++) {
        u64 phys = i * 1073741824ULL;
        if (i == 0) continue;

        u64 pml5_idx = phys / (1ULL << 48);
        u64 pml4_idx = (phys % (1ULL << 48)) / (1ULL << 39);
        u64 pdpt_idx = (phys % (1ULL << 39)) / 1073741824ULL;

        if (phys >= 0x0DF0000000ULL && phys <= 0x0E10000000ULL) {
            DebugStr("phys=");
            DebugU64(phys);
            DebugStr(" pml4_idx=");
            DebugU64(pml4_idx);
            DebugStr(" pdpt_idx=");
            DebugU64(pdpt_idx);
            DebugChar('\n');
        }

        if (!use_l5 && phys >= (1ULL << 48)) break;
        if (pml5_idx >= 512) break;

        u64* active_pml4 = pml4;
        if (use_l5) {
            if (root_table[pml5_idx] == 0) {
                alloc_addr = 0xFFFFFFFF;
                if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
                u64* new_pml4 = (u64*)alloc_addr;
                for (int k = 0; k < 512; k++) new_pml4[k] = 0;
                root_table[pml5_idx] = ((u64)new_pml4 & ~0xFFFULL) | 0x003;
            }
            active_pml4 = (u64*)(root_table[pml5_idx] & ~0xFFFULL);
        }

        if (active_pml4[pml4_idx] == 0) {
            alloc_addr = 0xFFFFFFFF;
            if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
            u64* new_pdpt = (u64*)alloc_addr;
            for (int k = 0; k < 512; k++) new_pdpt[k] = 0;
            active_pml4[pml4_idx] = ((u64)new_pdpt & ~0xFFFULL) | 0x003;
        }
        u64* cur_pdpt = (u64*)(active_pml4[pml4_idx] & ~0xFFFULL);

        if (pml5_idx == 0 && pml4_idx == 0 && pdpt_idx == 0) continue;

        if (phys > PhysMem || phys > MaxPhysMem) {
            cur_pdpt[pdpt_idx] = phys | 0x193; 
        } else {
            cur_pdpt[pdpt_idx] = phys | 0x083; 
        }
    }

    return root_table;
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

void InitPool(u64 PoolSize){
    AllocBlock *block = (AllocBlock*)POOL_PHYS_START;
    block->size=PoolSize - sizeof(AllocBlock);
    block->is_free=1;
    block->next = NULL;
    KernelPool.Head = block;
}

void InitBlock(u64 PoolSize,u64 Pos){
    AllocBlock *block = (AllocBlock*)Pos;
    block->size=PoolSize - sizeof(AllocBlock);
    block->is_free=1;
    block->next = NULL;
    PoolAddBlock(&KernelPool,block);
}

void LoadDynamicPageTable(u64* root_table) {
    u64 pure_cr3 = ((u64)root_table) & ~0xFFFULL;

    if (!IsLa57Supported()) {
        __asm__ volatile(
            "movq %0, %%cr3\n\t" 
            "movq %%cr3, %%rax\n\t"
            "jmp 1f\n\t"
            "1:\n\t"
            :: "r"(pure_cr3) : "memory"
        );
        return;
    }

    static u64 temporary_gdt[] = {
        0x0000000000000000,
        0x00209a0000000000, 
        0x00cf9a000000ffff, 
        0x00cf92000000ffff  
    };

    struct {
        u16 limit;
        u64 base;
    } __attribute__((packed)) gdtr = {
        .limit = sizeof(temporary_gdt) - 1,
        .base = (u64)temporary_gdt
    };

    __asm__ volatile(
        "movq %0, %%rbx\n\t"
        "lgdt %1\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq $0x10\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        
        ".code32\n\t"
        "1:\n\t"
        "movl $0x18, %%eax\n\t"
        "movl %%eax, %%ds\n\t"
        "movl %%eax, %%ss\n\t"
        
        "movl %%cr0, %%eax\n\t"
        "btrl $31, %%eax\n\t"
        "movl %%eax, %%cr0\n\t"
        
        "movl %%cr4, %%eax\n\t"
        "btsl $12, %%eax\n\t"
        "movl %%eax, %%cr4\n\t"
        
        "movl %%ebx, %%cr3\n\t"
        
        "movl %%cr0, %%eax\n\t"
        "btsl $31, %%eax\n\t"
        "movl %%eax, %%cr0\n\t"
        
        "pushl $0x08\n\t"
        "pushl $2f\n\t"
        "lretl\n\t"
        
        ".code64\n\t"
        "2:\n\t"
        "nop\n\t"
        :
        : "r"(pure_cr3), "m"(gdtr)
        : "rax", "rbx", "memory"
    );
}

u32 GetGopByWH(u64 W, u64 H)
{
    EFI_STATUS Status;
    u32 BestMode = (u32)-1;
    u64 BestDiff = ~0ULL;
    u64 TargetArea = W * H;

    if (Gop == NULL || Gop->Mode == NULL)
    {
        return BestMode;
    }

    for (u32 i = 0; i < Gop->Mode->MaxMode; i++)
    {
        EFI_GOP_MODE_INFO *Info = NULL;
        UINTN SizeOfInfo = 0;

        Status = Gop->QueryMode(Gop, i, &SizeOfInfo, &Info);
        if (EFI_ERROR(Status))
        {
            continue;
        }

        u64 CurrentArea = Info->HorizontalResolution * Info->VerticalResolution;
        u64 Diff = (CurrentArea > TargetArea) ? (CurrentArea - TargetArea) : (TargetArea - CurrentArea);

        DebugStr("W:");
        DebugU64(Info->HorizontalResolution);
        DebugStr(" H:");
        DebugU64(Info->VerticalResolution);
        DebugChar('\n');

        if (Diff < BestDiff)
        {
            BestDiff = Diff;
            BestMode = i;
        }
    }

    return BestMode;
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

    EFI_STATUS Status = Gop->SetMode(Gop, GetGopByWH(1280,720));
    if (status != EFI_SUCCESS)
    {
        DebugStr("\r\n[NeuroSamaOS] GOP Init Faild.\n");
        while(1) __asm__ volatile("cli\n\t hlt");
    }
    MemCopy(&GopMode,Gop->Mode,sizeof(EFI_GOP_MODE));
    MemCopy(&GopInfo,Gop->Mode->Info,sizeof(EFI_GOP_MODE_INFO));
    GopMode.FrameBufferSize=GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution * sizeof(HDR_PIXEL);
    GopOut=(u32*)(u64)GopMode.FrameBufferBase;
    DebugStr("\nGop->Mode->Info->PixelFormat = ");
    DebugU32(Gop->Mode->Info->PixelFormat);
    DebugChar('\n');

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
    u64 MaxAvailableAddress = 0;

    for (UINTN i = 0; i < EntryCount; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*)((u8*)MemMap + (i * DescriptorSize));
        u64 EndAddress = desc->PhysicalStart + (desc->NumberOfPages * 4096);
        
        if (EndAddress > MaxPhysicalAddress) {
            MaxPhysicalAddress = EndAddress;
        }
        
        if (desc->Type == EfiConventionalMemory || 
            desc->Type == EfiBootServicesData ||
            desc->Type == EfiRuntimeServicesData) {
            if (EndAddress > MaxAvailableAddress) {
                MaxAvailableAddress = EndAddress;
            }
        }
    }
    u64 RealMaxAddress = 0;
    u64 MaxMMIO = 0;

    for (UINTN i = 0; i < EntryCount; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*)((u8*)MemMap + (i * DescriptorSize));
        u64 EndAddress = desc->PhysicalStart + (desc->NumberOfPages * 4096);

        if (EndAddress > RealMaxAddress) {
            RealMaxAddress = EndAddress;
        }
        
        if (desc->Type == 11) {
            if (EndAddress > MaxMMIO) {
                MaxMMIO = EndAddress;
            }
        }
    }

    u64 MaxAddress = RealMaxAddress;
    if (MaxMMIO > MaxAddress) {
        MaxAddress = MaxMMIO;
    }
    if (MaxAddress < 0x0EFFFFFFFFULL) {
        MaxAddress = 0x0EFFFFFFFFULL;
        DebugChar('\n');
    }

    DebugStr("\nMaxPhysicalAddress: ");DebugU64(MaxAddress);DebugChar('\n');
    DebugStr("BuildDynamicPageTable.\n");
    u64 *pml5 = BuildDynamicPageTable(MaxAddress, MaxAvailableAddress, bs);
    if (!pml5) {
        DebugStr("BuildDynamicPageTable failed!\n");
        while(1) __asm__ volatile("cli\n\t hlt");
    }
    DebugStr("ExitBootServices.\n");
    DebugStr("ExitBootServices.\n");
    ExitBootServices_Safe(image);
    __asm__ volatile("cli\n\t");
    DebugStr("MemFlash.\n");
    MemFullFlash();
    DebugStr("Mov pml4 to cr3.\n");
    LoadDynamicPageTable(pml5);
    DebugStr("InitPool.\n");
    u64 PoolSize = 0x40000000;
    if (POOL_PHYS_START + PoolSize > MaxPhysicalAddress) {
        PoolSize = MaxPhysicalAddress - POOL_PHYS_START;
    }
    InitPool(PoolSize);
    DebugStr("AllocStack.\n");
    u8* KernelStack = AlignedAlloc(&KernelPool, 64*1024*1024,16);
    GopBack = (HDR_PIXEL*)(u64)AlignedAlloc(&KernelPool, GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution * sizeof(HDR_PIXEL), 64);

    LoadGDT();
    InitIDT();

    u64 StackTop = (u64)KernelStack + 64*1024*1024;
    u64 StackBottom = (u64)KernelStack;
    __asm__ volatile(
        "movq %0, %%rsp\n\t"
        "movq %1, %%rbp\n\t"
        "call kernel_main\n\t"
        :
        : "r"(StackTop), "r"(StackBottom)
        : "memory"
    );
    return 0;
}

void OnRequestComplete(UHCIRequest *req)
{
    DebugStr("Request completed: Addr=");
    DebugU64(req->DeviceAddress);
    DebugStr(" EP=");
    DebugU64(req->Endpoint);
    DebugStr(" Length=");
    DebugU64(req->ActualLength);
    DebugStr("\r\n");
}

void OnRequestError(UHCIRequest *req, UHCIResult error)
{
    DebugStr("Request error: ");
    DebugU64(error);
    DebugStr(" Addr=");
    DebugU64(req->DeviceAddress);
    DebugStr("\r\n");
}

void OnPortChange(u8 port, u16 status)
{
    DebugStr("Port ");
    DebugU64(port);
    DebugStr(" changed: 0x");
    DebugU64(status);
    DebugStr("\r\n");
}

__attribute__((force_align_arg_pointer)) void kernel_main() {
    DebugStr("Jumped To KernelMain.\n");
    GopMode.FrameBufferSize=GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution * sizeof(HDR_PIXEL);
    DebugStr("GopBack = ");
    DebugU64((u64)GopBack);
    DebugChar('\n');
    DebugStr("GopOut = ");
    DebugU64((u64)GopOut);
    DebugChar('\n');
    DebugStr("PixelsPerScanLine = ");
    DebugU32(GopInfo.PixelsPerScanLine);
    DebugChar('\n');
    DebugStr("VerticalResolution = ");
    DebugU32(GopInfo.VerticalResolution);
    DebugChar('\n');
    DebugStr("pixelCount = ");
    DebugU32(GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution);
    DebugChar('\n');
    DebugStr("PixelFormat = ");
    DebugU32(GopInfo.PixelFormat);
    DebugChar('\n');
    
    DebugStr("Init ATA.\n");
    ATAInit();
    DebugStr("ATA Init Done.\n");
    
    u32 esp_count = GPTGetESPCount();
    DebugStr("ESP count: ");
    DebugU32(esp_count);
    DebugChar('\n');

    for (u32 i = 0; i < esp_count; i++) {
        ESPEntry* esp = GPTGetESP(i);
        if (esp) {
            DebugStr("ESP ");
            DebugU32(i);
            DebugStr(": LBA=");
            DebugU64(esp->StartLba);
            DebugStr(" Channel=");
            DebugU8(esp->Channel);
            DebugStr(" Drive=");
            DebugU8(esp->Drive);
            DebugChar('\n');
        }
    }
    
    DebugStr("If in QEMU,All int 0xD Bug is QEMU Bug,Not Kernel:\nInit GTX960.\n");
    NvidiaGPU GPU={0};
    if (PCIFindNvidiaGPU(&GPU)) {
        if (NvidiaGPUInit(&GPU)) {
            DebugStr("GTX960 Init Success.\n");
        } else {
            DebugStr("GTX960 Init Failed.\n");
        }
    } else {
        DebugStr("GTX960 Not Found.\n");
    }
    DebugStr("Init GTX960 Finish. End,Now All int Bug is Kernel Bug\n");

    DebugStr("Init UHCI.\n");
    UHCIHostController *hc=NULL;
    UHCIContext *ctx=NULL;
    _Bool UhciFound = 0;
    for (u16 bus = 0; bus < 256; bus++)
    {
        for (u16 slot = 0; slot < 32; slot++)
        {
            for (u16 func = 0; func < 8; func++)
            {
                u32 VendorDevice = PCIReadDWORD(bus, slot, func, 0x00);
                u16 vendor = VendorDevice & 0xFFFF;

                if (vendor == 0xFFFF)
                    continue;

                u32 ClassRev = PCIReadDWORD(bus, slot, func, 0x08);
                u8 ClassCode = (ClassRev >> 24) & 0xFF;
                u8 SubClass = (ClassRev >> 16) & 0xFF;
                u8 ProgIF = (ClassRev >> 8) & 0xFF;

                if (ClassCode == UHCI_CLASS_CODE &&
                    SubClass == UHCI_SUBCLASS &&
                    ProgIF == UHCI_INTERFACE)
                {
                    DebugStr("Found UHCI: Bus=");
                    DebugU64(bus);
                    DebugStr(" Slot=");
                    DebugU64(slot);
                    DebugStr(" Func=");
                    DebugU64(func);
                    DebugStr("\r\n");

                    hc = UHCICreate(bus, slot, func, &KernelPool);
                    if (!hc)
                    {
                        DebugStr("Failed to create UHCI controller\r\n");
                        return;
                    }

                    ctx = Alloc(&KernelPool,sizeof(UHCIContext));
                    if (!ctx)
                        return;
                    MemSet(ctx, 0, sizeof(UHCIContext));

                    ctx->HC = hc;
                    ctx->MemoryPool = &KernelPool;
                    ctx->HandleCompletion = OnRequestComplete;
                    ctx->HandleError = OnRequestError;
                    ctx->HandlePortChange = OnPortChange;

                    UHCIResult result = UHCIInitialize(ctx);
                    if (result != UHCI_OK)
                    {
                        DebugStr("UHCI init failed: ");
                        DebugU64(result);
                        DebugStr("\r\n");
                        return;
                    }

                    UHCIConfigure(ctx, 1, 0);

                    UHCIStart(ctx);

                    DebugStr("UHCI initialized and started\r\n");
                    UhciFound=1;
                }
            }
        }
    }

    DebugStr("init XHCI.\n");
    XhciController *Xhci = XhciInit(GetXhciMmioBase());
    if (Xhci)
    {
        XhciScanPorts(Xhci);
        DebugStr("XhciScanPorts Finished.\n");
    }

    MouseDevice* XhciMouse=MouseInit(Xhci);
    KeyboardDevice* XhciKerboard=KeyboardInit(Xhci);
    if(UhciFound){
        USBMouse* UhciMouse=NULL;
        USBKeyboard* UhciKeyboard=NULL;
        for (u8 port = 0; port < 2; port++) {
            u16 portStatus = UHCIGetPortStatus(ctx, port);
            
            if (portStatus & UHCI_PORTSC_CCS) {
                DebugStr("Device connected on port ");
                DebugU64(port);
                DebugChar('\n');
                
                UhciMouse = USBMouseInit(ctx, port);
                if (UhciMouse) {
                    DebugStr("Mouse initialized on port ");
                    DebugU64(port);
                    DebugChar('\n');
                } else {
                    UhciKeyboard = USBKeyboardInit(ctx, port);
                    if (UhciKeyboard) {
                        DebugStr("Keyboard initialized on port ");
                        DebugU64(port);
                        DebugChar('\n');
                    }
                }
            }
        }
        if (!UhciMouse) DebugStr("No Uhci Mouse Drive.\n");
        if (!UhciKeyboard) DebugStr("No Uhci Keyboard Drive.\n");
    }

    while (1)
        {
            GOPClear(HDR_Pack(0x0000, 0x0000, 0x0000, 0x0000));
            TaskPoll();
            GOPRectFill((u32[2]){0, GopInfo.HorizontalResolution}, (u32[2]){0, GopInfo.VerticalResolution >> 5}, HDR_Pack(0x0000, 0xFF00, 0xFF00, 0x7FFF));
            GOPFlash();
        }
}