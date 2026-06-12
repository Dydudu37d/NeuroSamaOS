#include "efi.h"
#include "gop.h"
#include "str.h"
#include "debug.h"
#include "clock.h"
#include "pci.h"
#include "xhci.h"

u32* GopOut=NULL;
u32* GopBack=NULL;

EFI_BOOT_SERVICES *bs=NULL;
EFI_GOP_MODE GopMode={0};
EFI_GOP_MODE_INFO GopInfo={0};
EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop=NULL;

EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

static void SIMDInit(void) {
    unsigned long long cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    
    cr4 |= (1 << 9)
         | (1 << 10)
         | (1 << 18);
    
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
    
    unsigned long long xcr0 = 0;
    __asm__ volatile ("xgetbv" : "=A"(xcr0) : "c"(0));
    
    xcr0 |= (1 << 1)
          | (1 << 2);
    
    __asm__ volatile ("xsetbv" : : "c"(0), "A"(xcr0));
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

void kernel_main(){
    while (1){

    }
}

__attribute__((used, retain, visibility("default")))
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *sys) {
    bs=sys->BootServices;
    SIMDInit();
    DebugInit();
    InitClock();
    
    DebugStr("NeuroSamaOS.UEFI Boot Time:\n");
    DebugStr("GOP Init.\n");
    EFI_STATUS Status = bs->LocateProtocol(&GopGuid, NULL, (void **)&Gop);
    DebugStr("GOP(Left low Right high):");
    DebugU64Bit(Status);
    DebugChar('\n');
    Gop->SetMode(Gop,0);

    GopMode.Mode=Gop->Mode->Mode;
    GopMode.MaxMode=Gop->Mode->MaxMode;
    GopMode.FrameBufferBase=Gop->Mode->FrameBufferBase;
    GopMode.FrameBufferSize=Gop->Mode->FrameBufferSize;
    GopMode.SizeOfInfo=Gop->Mode->SizeOfInfo;
    GopMode.Info=Gop->Mode->Info;
    GopInfo.PixelFormat=GopMode.Info->PixelFormat;
    GopInfo.PixelsPerScanLine=GopMode.Info->PixelsPerScanLine;
    GopInfo.PixelInformation=GopMode.Info->PixelInformation;
    GopInfo.HorizontalResolution=GopMode.Info->HorizontalResolution;
    GopInfo.VerticalResolution=GopMode.Info->VerticalResolution;
    GopInfo.Version=GopMode.Info->Version;
    
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
    
    DebugStr("\nExitBootServices Status:");
    DebugU64(ExitBootServices_Safe(image));
    DebugChar('\n');

    GOPClear(0x00FFABC1);
    GOPFlash();
    

    return 0;
}
