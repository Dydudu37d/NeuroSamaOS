#include "efi.h"

EFI_STATUS EFIAPI efi_main(void *image, EFI_SYSTEM_TABLE *sys) {
    EFI_BOOT_SERVICES *bs=sys->BootServices;
    
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *TextConOut=sys->ConOut;
    TextConOut->OutputString(TextConOut,(u16*)L"Hello World\r\nTest.");

    while (1);

    return 0;
}
