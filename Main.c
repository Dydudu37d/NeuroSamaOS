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
#include "hda.h"
#include "gm206.h"
#include "clock.h"
#include "xhci.h"
#include "math.h"

#define UHCI_LOW_MEMORY_BASE 0x10000
#define UHCI_LOW_MEMORY_SIZE (32 * 1024 * 1024)

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_LARGE    (1ULL << 7)
#define PAGE_HUGE     (1ULL << 7)

#define KERNEL_POOL_BASE 0x1100

void* GopOut=NULL;
HDR_PIXEL* GopBack=NULL;

EFI_BOOT_SERVICES *bs=NULL;
EFI_GOP_MODE GopMode={0};
EFI_GOP_MODE_INFO GopInfo={0};
EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop=NULL;

EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
RegContext MainGlobalContext={0};

const size_t KERNEL_POOL_SIZE = (1<<30);
const size_t KERNEL_POOL_PAGES = EFI_SIZE_TO_PAGES(KERNEL_POOL_SIZE);

u8* KernelStack = NULL;
u8 KernelPoolHeadList[KERNEL_POOL_SIZE*sizeof(AllocBlock)];

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
    if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
    u64* root_table = (u64*)alloc_addr;
    MemSet(root_table, 0, 4096);

    u64* pml4 = NULL;
    if (use_l5) {
        alloc_addr = 0xFFFFFFFF;
        if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
        pml4 = (u64*)alloc_addr;
        MemSet(pml4, 0, 4096);
        root_table[0] = ((u64)pml4 & ~0xFFFULL) | 0x003;
    } else {
        pml4 = root_table;
    }

    if (pml4[0] == 0) {
        alloc_addr = 0xFFFFFFFF;
        if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
        u64* pdpt = (u64*)alloc_addr;
        MemSet(pdpt, 0, 4096);
        pml4[0] = ((u64)pdpt & ~0xFFFULL) | 0x003;
    }
    u64* pdpt_low = (u64*)(pml4[0] & ~0xFFFULL);

    alloc_addr = 0xFFFFFFFF;
    if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
    u64* pd_low = (u64*)alloc_addr;
    MemSet(pd_low, 0, 4096);
    pdpt_low[0] = ((u64)pd_low & ~0xFFFULL) | 0x003;

    alloc_addr = 0xFFFFFFFF;
    if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
    u64* pt_4k = (u64*)alloc_addr;
    MemSet(pt_4k, 0, 4096);
    for (int i = 0; i < 512; i++) {
        pt_4k[i] = (i * 0x1000) | 0x003;
    }
    pd_low[0] = ((u64)pt_4k & ~0xFFFULL) | 0x003;

    for (int i = 1; i < 512; i++) {
        pd_low[i] = (i * 0x200000) | 0x183;
    }

    Cu128 max_phys = (Cu128)MaxPhysMem;
    Cu128 divisor = (Cu128)0x40000000ULL;
    Cu128 result = (max_phys + divisor - 1) / divisor;
    u64 max_1gb_pages_limit = use_l5 ? 262144 : 512;
    u64 max_1gb_pages = (result > max_1gb_pages_limit) ? max_1gb_pages_limit : (u64)result;

    for (u64 i = 1; i < max_1gb_pages; i++) {
        u64 phys = i * 0x40000000ULL;
        u64 pml5_idx = phys / (1ULL << 48);
        u64 pml4_idx = (phys % (1ULL << 48)) / (1ULL << 39);
        u64 pdpt_idx = (phys % (1ULL << 39)) / 0x40000000ULL;

        if (!use_l5 && phys >= (1ULL << 48)) break;
        if (pml5_idx >= 512) break;

        u64* active_pml4 = pml4;
        if (use_l5) {
            if (root_table[pml5_idx] == 0) {
                alloc_addr = 0xFFFFFFFF;
                if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
                u64* new_pml4 = (u64*)alloc_addr;
                MemSet(new_pml4, 0, 4096);
                root_table[pml5_idx] = ((u64)new_pml4 & ~0xFFFULL) | 0x003;
            }
            active_pml4 = (u64*)(root_table[pml5_idx] & ~0xFFFULL);
        }

        if (active_pml4[pml4_idx] == 0) {
            alloc_addr = 0xFFFFFFFF;
            if (bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, 1, &alloc_addr) != EFI_SUCCESS) return 0;
            u64* new_pdpt = (u64*)alloc_addr;
            MemSet(new_pdpt, 0, 4096);
            active_pml4[pml4_idx] = ((u64)new_pdpt & ~0xFFFULL) | 0x003;
        }
        u64* cur_pdpt = (u64*)(active_pml4[pml4_idx] & ~0xFFFULL);
        cur_pdpt[pdpt_idx] = phys | 0x183;
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

void InitPool(AllocPool* Pool, u64 PoolSize, void* Start){
    if (!Pool || !Start) return;
    AllocBlock *block = (AllocBlock*)Start;
    block->size = PoolSize - sizeof(AllocBlock);
    block->is_free = 1;
    block->next = NULL;
    block->prev = NULL;
    Pool->Head = block;
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
    InitClock();

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
    if (Status != EFI_SUCCESS)
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
    u64 LowAvailableAddress = 0;

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
            if (desc->PhysicalStart < 0x100000000ULL && desc->PhysicalStart > 0x100000) {
                if (LowAvailableAddress == 0 || desc->PhysicalStart < LowAvailableAddress) {
                    LowAvailableAddress = desc->PhysicalStart;
                }
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
    if (0xFFFFFFFFFFFF > MaxAddress) {
        MaxAddress = 0xFFFFFFFFFFFF;
    }

    DebugStr("\nMaxPhysicalAddress: ");DebugU64(MaxAddress);DebugChar('\n');
    DebugStr("BuildDynamicPageTable.\n");
    u64 *pml5 = BuildDynamicPageTable(MaxAddress, MaxAvailableAddress, bs);
    if (!pml5) {
        DebugStr("BuildDynamicPageTable failed!\n");
        while(1) __asm__ volatile("cli\n\t hlt");
    }
    
    EFI_PHYSICAL_ADDRESS stack_alloc_addr = MaxAvailableAddress;
    status = bs->AllocatePages(AllocateMaxAddress, EfiRuntimeServicesData, EFI_SIZE_TO_PAGES(128 * 1024 * 1024), &stack_alloc_addr);
    if (status != EFI_SUCCESS) {
        DebugStr("Allocate KernelStack failed!\n");
        while(1) __asm__ volatile("cli\n\t hlt");
    }
    KernelStack = (u8*)stack_alloc_addr;
    
    DebugStr("InitPool.\n");
    
    DebugStr("allocated at 0x");
    DebugU64((u64)KernelPoolHeadList);
    DebugStr("\n");
    DebugStr("KERNEL_POOL_PAGES = ");
    DebugU64(KERNEL_POOL_PAGES);
    DebugChar('\n');
    InitPool(&KernelPool, KERNEL_POOL_SIZE, KernelPoolHeadList);
    DebugStr("KernelPool initialized at 0x");
    DebugU64((u64)KernelPool.Head);
    DebugStr("\n");
    
    DebugStr("ExitBootServices.\n");
    ExitBootServices_Safe(image);
    __asm__ volatile("cli\n\t");
    DebugStr("MemFlash.\n");
    MemFullFlash();
    u64 StackTop = (u64)KernelStack + 128 * 1024 * 1024;
    __asm__ volatile(
        "movq %0, %%rsp\n\t"
        "xorq %%rbp, %%rbp\n\t"
        :: "r"(StackTop) : "memory"
    );
    DebugStr("Mov pml4 to cr3.\n");
    LoadDynamicPageTable(pml5);
    DebugStr("AllocStack.\n");
    GopBack = (HDR_PIXEL*)(u64)AlignedAlloc(&KernelPool, GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution * sizeof(HDR_PIXEL), 64);

    LoadGDT();
    InitIDT();

    __asm__ volatile(
        "call kernel_main\n\t"
        :
        :
        : "memory"
    );
    return 0;
}

__attribute__((force_align_arg_pointer)) void kernel_main() {
    DebugStr("Jumped To KernelMain.\n");
    
    if (!KernelPool.Head) {
        DebugStr("ERROR: KernelPool.Head is NULL!\n");
        while(1) __asm__ volatile("cli\n\t hlt");
    }
    
    DebugStr("KernelPool.Head at 0x");
    DebugU64((u64)KernelPool.Head);
    DebugStr(" size: ");
    DebugU64(KernelPool.Head->size);
    DebugStr(" is_free: ");
    DebugU8(KernelPool.Head->is_free);
    DebugStr(" Next block size: ");
    if (KernelPool.Head->next) {
        DebugU64(KernelPool.Head->next->size);
    } else {
        DebugStr("NULL");
    }
    DebugChar('\n');

    if (!GopBack) {
        DebugStr("ERROR: GopBack is NULL!\n");
        while(1) __asm__ volatile("cli\n\t hlt");
    }
    
    GopMode.FrameBufferSize = GopInfo.PixelsPerScanLine * GopInfo.VerticalResolution * sizeof(HDR_PIXEL);
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
    MemFullFlash();
    GOPClear(HDR_Pack(0,0,0,0));
    GOPRectFill((u32[2]){0,100},(u32[2]){0,100},HDR_Pack(0xFFFF,0xFFFF,0x0000,0xFFFF));
    GOPFlash();

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
    HDAudio* KernelHDAudio = HDAudioInit();
    DebugStr("KernelHDAudio At:");DebugU64((u64)KernelHDAudio);DebugChar('\n');

    DebugStr("Loop.\n");
    while (1){
        GOPClear(HDR_Pack(0x0000, 0x0000, 0x0000, 0x7FFF));
        TaskPoll();

        GOPRectFill((u32[2]){0, GopInfo.HorizontalResolution}, (u32[2]){0, GopInfo.VerticalResolution >> 5}, HDR_Pack(0x0000, 0xFFFF, 0xFFFF, 0x7FFF));
        GOPFlash();
    }
}