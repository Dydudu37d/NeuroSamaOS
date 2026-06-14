#include "efi.h"
#include "flash.h"
#include "gop.h"
#include "int.h"
#include "str.h"
#include "debug.h"
#include "clock.h"
#include "pci.h"
#include "xhci.h"
#include "kmalloc.h"
#include "gdt.h"
#include "idt.h"
#include "Context.h"
#include "task.h"
#include "mouse.h"
#include "keyboard.h"
#include "ohci.h"
#include "hda.h"

#define POOL_PHYS_START 0x200000ULL
#define POOL_SIZE       0x80000000ULL

u32* GopOut=NULL;
u32* GopBack=NULL;

EFI_BOOT_SERVICES *bs=NULL;
EFI_GOP_MODE GopMode={0};
EFI_GOP_MODE_INFO GopInfo={0};
EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop=NULL;

EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
RegContext MainGlobalContext={0};

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
        
        if (EFI_ERROR(status)) {
            return status; 
        }
        status = bs->ExitBootServices(ImageHandle, MapKey);
        
        if (status == EFI_SUCCESS) {
            break;
        }
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
                    DebugStr("FOUND XHCI AT ");
                    DebugU32(bus); DebugStr(":"); DebugU32(dev); DebugStr("."); DebugU32(func); DebugStr("\n");

                    u64 bar = PCIGetBARAddress(bus, dev, func, 0);
                    DebugStr("XHCI BAR0 address: ");
                    DebugU64(bar);
                    DebugStr("\n");
                    
                    PCIEnableDevice(bus, dev, func);
                    return bar;
                }
            }
        }
    }
    return 0;
}

u64 GetOhciMmioBase() {
    for (u16 bus = 0; bus < 256; bus++) {
        for (u8 dev = 0; dev < 32; dev++) {
            for (u8 func = 0; func < 8; func++) {
                u32 id = PCIReadDWORD(bus, dev, func, 0x00);
                if (id == 0xFFFFFFFF) continue;

                u32 classRev = PCIReadDWORD(bus, dev, func, 0x08);
                u8 class = (classRev >> 24) & 0xFF;
                u8 subclass = (classRev >> 16) & 0xFF;
                u8 progIf = (classRev >> 8) & 0xFF;

                if (class == 0x0C && subclass == 0x03 && progIf == 0x10) {
                    DebugStr("FOUND OHCI AT ");
                    DebugU32(bus); DebugStr(":"); DebugU32(dev); DebugStr("."); DebugU32(func); DebugStr("\n");
                    
                    u32 barSize = PCIGetBARSize64(bus, dev, func, 0);
                    DebugStr("OHCI BAR0 size: ");
                    DebugU32(barSize);
                    DebugStr("\n");
                    PCIEnableDevice(bus, dev, func);
                    return PCIGetBARAddress(bus, dev, func, 0);
                }
            }
        }
    }
    return 0;
}

void KernelDrawCur(u64 x,u64 y,u32 color){
    GOPLine((u32[2]){x,x},(u32[2]){y,y+10},color);
    GOPLine((u32[2]){x,x+10},(u32[2]){y,y},color);
    GOPLine((u32[2]){x,x+20},(u32[2]){y,y+20},color);
}

void kernel_main() {
    u64 XhciMMIO = GetXhciMmioBase();
    XhciController* Xhci = XhciInit(XhciMMIO);
    if (Xhci) XhciScanPorts(Xhci);

    KeyboardDevice* kbd = NULL;
    MouseDevice* mouse = NULL;

    DebugStr("Horizontal: "); DebugU32(GopInfo.HorizontalResolution); DebugChar('\n');
    DebugStr("Vertical: "); DebugU32(GopInfo.VerticalResolution); DebugChar('\n');
    
    TaskAdd((Task){.Active=true,.IntervalNs=100,.Arg=(void*)Xhci,.CallFunc=(void*)XhciPollEvents,.NextWaitNs=0,.Name="XHCI_POLL" }, 1);

    u32 cur_x=0,cur_y=0;
    while(1){
        TaskPoll();
        GOPClear(0xFFFF0000);

        s8 dx=0,dy=0;
        u8 buttons=0;

        char c=0;
        if (kbd) {
            KeyboardPoll(kbd);
            while (KeyboardHasChar(kbd)) {
                c = KeyboardGetChar(kbd);
            }
        }
        
        if (mouse) {
            MousePoll(mouse);
            s16 dx, dy;
            u8 buttons;
            MouseGetDelta(mouse, &dx, &dy, &buttons);
        }

        if (dx > 0 && cur_x < GopInfo.HorizontalResolution - 1) cur_x += dx;
        if (dx < 0 && cur_x > 0) cur_x += dx;
        if (dy > 0 && cur_y > 0) cur_y -= dy;
        if (dy < 0 && cur_y < GopInfo.VerticalResolution - 1) cur_y -= dy;

        KernelDrawCur(cur_x, cur_y, 0xFFABCDEF);

        GOPFlash();
    }
}

void InitPool(){
    AllocBlock *block = (AllocBlock*)POOL_PHYS_START;

    block->size=POOL_SIZE - sizeof(AllocBlock);
    block->is_free=1;
    block->next = NULL;

    KernelPool.Head = block;
}

EFI_STATUS EFIAPI Cefi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *sys) {
    bs=sys->BootServices;
    SIMDInit();
    FPUInit();
    DebugInit();
    InitClock();
    
    DebugStr("NeuroSamaOS.UEFI Boot Time:\n");
    DebugStr("GOP Init.\n");
    EFI_STATUS Status = bs->LocateProtocol(&GopGuid, NULL, (void **)&Gop);
    DebugStr("GOP(Left bit0 Right bit64):");
    DebugU64Bit(Status);
    DebugChar('\n');
    Gop->SetMode(Gop,0);

    MemCopy(&GopMode,Gop->Mode,sizeof(EFI_GOP_MODE));
    MemCopy(&GopInfo,Gop->Mode->Info,sizeof(EFI_GOP_MODE_INFO));
    
    DebugStr("Pixel Format is: ");
    if (GopInfo.PixelFormat == 0) {
        DebugStr("BGR\n");
    } else if (GopInfo.PixelFormat == 1) {
        DebugStr("RGB\n");
    } else if (GopInfo.PixelFormat == 2) {
        DebugStr("BitMask\n");
    }
    
    u32 FrameBufferSize = GopMode.FrameBufferSize;
    u32 PagesNeeded = (FrameBufferSize + 4095) / 4096;
    GopOut = (u32*)(u64)GopMode.FrameBufferBase;
    EFI_PHYSICAL_ADDRESS GopBackReal;
    EFI_STATUS s = bs->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 
                  PagesNeeded, &GopBackReal);
    DebugU64(s);
    GopBack = (u32*)(u64)GopBackReal;
    
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemMap = NULL;
    UINTN MapKey;
    UINTN DescriptorSize;
    u32 DescriptorVersion;
    EFI_STATUS status;

    bs->GetMemoryMap(&MemoryMapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    MemoryMapSize += 2 * DescriptorSize;
    status = bs->AllocatePool(EfiLoaderData, MemoryMapSize, (void**)&MemMap); 
    status = bs->GetMemoryMap(&MemoryMapSize, (void*)MemMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    
    if (EFI_ERROR(status)) {
        DebugStr("Failed to get memory map!\n");
        while (1);
    }

    UINTN EntryCount = MemoryMapSize / DescriptorSize;
    u64 MaxPhysicalAddress = 0;

    DebugStr("--- MEMORY MAP DETECTION ---\n");
    
    for (UINTN i = 0; i < EntryCount; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*)((u8*)MemMap + (i * DescriptorSize));
        u64 EndAddress = desc->PhysicalStart + (desc->NumberOfPages * 4096);
        
        DebugStr("Type: "); DebugU64(desc->Type);
        DebugStr(" Start: "); DebugU64(desc->PhysicalStart);
        DebugStr(" End: "); DebugU64(EndAddress);
        DebugChar('\n');

        if (desc->Type == 7) { 
            if (EndAddress > MaxPhysicalAddress) {
                MaxPhysicalAddress = EndAddress;
            }
        }
    }

    DebugStr("Max Usable Physical Memory Address: ");
    DebugU64(MaxPhysicalAddress);
    DebugStr("\n----------------------------\n");

    bs->FreePool(MemMap);

    DebugStr("\nExitBootServices Status:");
    DebugU64(ExitBootServices_Safe(image));
    DebugChar('\n');
    asm volatile("cli");

    u64 *Pml4 = (u64*)0x1000000;
    u64 *Pdpt = (u64*)0x1001000;
    u64 *Pde = (u64*)0x1002000;
    
    MemSet64(Pml4, 0, 512);
    MemSet64(Pdpt, 0, 512);
    MemSet64(Pde, 0, 512);
    
    Pml4[0] = (u64)Pdpt | 0x03;
    for (u64 i = 0; i < 512; i++) {
        Pdpt[i] = (i * 0x40000000ULL) | 0x9B;
    }
    
    u64 *PdptHigh = (u64*)0x1003000;
    MemSet64(PdptHigh, 0, 512);
    Pml4[1] = (u64)PdptHigh | 0x03;
    for (u64 i = 0; i < 512; i++) {
        PdptHigh[i] = ((512 + i) * 0x40000000ULL) | 0x9B;
    }
    
    u64 *PdptHigh2 = (u64*)0x1004000;
    MemSet64(PdptHigh2, 0, 512);
    Pml4[2] = (u64)PdptHigh2 | 0x03;
    for (u64 i = 0; i < 512; i++) {
        PdptHigh2[i] = ((1024 + i) * 0x40000000ULL) | 0x9B;
    }
    
    asm volatile (
        "movq %0, %%cr3\n"
        "movq %%cr4, %%rax\n"
        "orq $0x20, %%rax\n"
        "movq %%rax, %%cr4\n"
        "movq %%cr0, %%rax\n"
        "bts $31, %%rax\n"
        "movq %%rax, %%cr0\n"
        : : "r"(Pml4) : "rax", "memory"
    );
    InitPool();
    

    DebugStr("Kernel Stack Alloc:");
    u8* KernelStack=Alloc(&KernelPool,256*1024*1024);
    if (!KernelStack){
        DebugStr("NULL(Nooooooo!!!!!!!!!!!!!!)\n");
        while(1);
    }else{
        DebugU64((u64)KernelStack);
        DebugStr("(Yeahhhhhhhhhhhh!!!!!!!!!!!)\n");
    }
    
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    
    u64 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    DebugStr("CR0 (Binary,Left bit0 Right bit63): ");
    DebugU64Bit(cr0);
    DebugChar('\n');
    DebugU64(cr0);
    DebugChar('\n');

    SaveContext(&MainGlobalContext);

    asm volatile("cli;cli");
    LoadGDT();
    InitIDT();
    DebugStr("Int7Test:");
    // asm volatile("int $7\n");
    DebugStr("\nTurn Stack and jump to kernel_main.\n");
    u64 StackTop = (u64)KernelStack + 256*1024*1024;
    asm volatile(
        "movq %0, %%rsp\n"
        "movq %0, %%rbp\n"
        "jmp kernel_main\n"
        : : "r"(StackTop) : "memory"
    );
    return 0;
}
