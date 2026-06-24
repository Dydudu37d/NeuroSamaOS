#include "gm206.h"
#include "pci.h"
#include "int.h"

_Bool PCIFindNvidiaGPU(NvidiaGPU *GPU){
    for (u16 Bus=0;Bus<256;Bus++){
    for (u8 Dev=0;Dev<32;Dev++){
    for (u8 Func=0;Func<8;Func++){
        u16 Vender=PCIReadWORD(Bus,Dev,Func,0x00);
        if (Vender==0xFFFF){if (Func==0) break;continue;}
        u16 Device=PCIReadWORD(Bus, Dev, Func, 0x02);
        if (Vender==NVIDIA_VENDOR_ID && Device==GTX960_DEVICE_ID){
            GPU->Bus=Bus;GPU->Slot=Dev;GPU->Func=Func;
            return 1;
        }
    }}}
    return 0;
}

u64 PCIGetNvidiaGPUBar64(u8 Bus, u8 Slot, u8 Func, u8 BARIndex){
    return PCIGetBARAddress(Bus, Slot, Func, BARIndex);
}

u64 PCIGetNvidiaGPUBarSize64(u8 Bus, u8 Slot, u8 Func, u8 BARIndex){
    return PCIGetBARSize64(Bus, Slot, Func, BARIndex);
}

_Bool NvidiaGPUInit(NvidiaGPU *GPU){
    PCIEnableDevice(GPU->Bus, GPU->Slot, GPU->Func);
    GPU->Bar0Base=PCIGetBARAddress(GPU->Bus, GPU->Slot, GPU->Func, 0);
    GPU->Bar0Size=PCIGetBARSize64(GPU->Bus, GPU->Slot, GPU->Func, 0);
    GPU->BootStatus=NvidiaGPURead32(GPU, NVIDIA_PMC_BOOT_0);
    NvidiaGPUWrite32(GPU,NVIDIA_PMC_ENABLE,NVIDIA_PMC_ENABLE_ENABLE);
    u32 GrStatus=0;
    for (u64 i=0;i<10000000;i++){
        GrStatus=NvidiaGPURead32(GPU, 0x00400000);
        if (GrStatus&0x00000001) break;
        asm volatile("pause");
    }
    GPU->GrStatus=GrStatus;
    GPU->VramSize=NvidiaGPURead32(GPU, 0x00200000)&0xFFFFFFF0;
    GPU->VramBase=PCIGetNvidiaGPUBarSize64(GPU->Bus, GPU->Slot, GPU->Func, 1) ;
    return GPU->BootStatus==NVIDIA_BOOTED;
}
