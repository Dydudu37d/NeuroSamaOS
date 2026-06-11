#include "efi.h"
#include "gop.h"
#include "str.h"
#include "debug.h"

u32* GopOut=NULL;
u32* GopBack=NULL;

EFI_BOOT_SERVICES *bs=NULL;
EFI_GOP_MODE *GopMode=NULL;
EFI_GOP_MODE_INFO *GopInfo=NULL;
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

EFI_STATUS EFIAPI efi_main(void *image, EFI_SYSTEM_TABLE *sys) {
    SIMDInit();
    DebugInit();
    bs=sys->BootServices;
    
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *TextConOut=sys->ConOut;
    TextConOut->OutputString(TextConOut,(u16*)L"NeuroSamaOS.UEFI Boot Time:\r\n");
    TextConOut->OutputString(TextConOut,(u16*)L"GOP Init.\r\n");
    EFI_STATUS Status = bs->LocateProtocol(&GopGuid, NULL, (void **)&Gop);
    TextConOut->OutputString(TextConOut,(u16*)L"GOP(Left low Right high):");
    for (u64 idx=0;idx<65;idx++){
        u16 Temp[2]={0};
        Temp[0] = (Status & (1ULL << idx)) ? '1' : '0';
        Temp[1] = '\0';
        TextConOut->OutputString(TextConOut,Temp);
    }
    TextConOut->OutputString(TextConOut,(u16*)L".\r\n");
    Gop->SetMode(Gop,0);

    GopMode=Gop->Mode;
    GopInfo=GopMode->Info;
    
    TextConOut->OutputString(TextConOut, (u16*)L"Pixel Format is: ");
    DebugStr("Pixel Format is: ");
    if (GopInfo->PixelFormat == 0) {
        TextConOut->OutputString(TextConOut, (u16*)L"BGR\r\n");
        DebugStr("BGR\n");
    } else if (GopInfo->PixelFormat == 1) {
        TextConOut->OutputString(TextConOut, (u16*)L"RGB\r\n");
        DebugStr("RGB\n");
    } else if (GopInfo->PixelFormat == 2) {
        TextConOut->OutputString(TextConOut, (u16*)L"BitMask\r\n");
        DebugStr("BitMask\n");
    }

    u32 FrameBufferSize = GopMode->FrameBufferSize;
    u32 PagesNeeded = (FrameBufferSize + 4095) / 4096;
    GopOut = (u32*)(u64)GopMode->FrameBufferBase;
    EFI_PHYSICAL_ADDRESS GopBackReal;
    EFI_STATUS s = bs->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 
                  PagesNeeded, &GopBackReal);
    DebugU64(s);
    GopBack = (u32*)(u64)GopBackReal;
    
    GOPClear(0x00FFABC1);
    GOPFlash();
    
    

    while (1);

    return 0;
}
