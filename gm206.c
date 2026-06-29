#include "gm206.h"
#include "pci.h"
#include "int.h"
#include "debug.h"

static _Bool PCIFindNvidiaGPU_Recursive(u8 Bus, NvidiaGPU *GPU) {
    for (u8 Dev = 0; Dev < 32; Dev++) {
        u32 VendorDevice0 = PCIReadDWORD(Bus, Dev, 0, 0x00);
        if ((VendorDevice0 & 0xFFFF) == 0xFFFF) continue;
        
        u8 HeaderType = PCIReadBYTE(Bus, Dev, 0, 0x0E);
        u8 MaxFunc = (HeaderType & 0x80) ? 8 : 1;
        
        for (u8 Func = 0; Func < MaxFunc; Func++) {
            u32 VendorDevice = PCIReadDWORD(Bus, Dev, Func, 0x00);
            u16 Vendor = VendorDevice & 0xFFFF;
            if (Vendor == 0xFFFF) continue;
            
            u16 Device = (VendorDevice >> 16) & 0xFFFF;
            if (Vendor == NVIDIA_VENDOR_ID && Device == GTX960_DEVICE_ID) {
                GPU->Bus = Bus; GPU->Slot = Dev; GPU->Func = Func;
                return 1;
            }
            
            if (PCIIsBridge(Bus, Dev, Func)) {
                u8 SecondaryBus = PCIGetSecondaryBus(Bus, Dev, Func);
                if (SecondaryBus != Bus) {
                    if (PCIFindNvidiaGPU_Recursive(SecondaryBus, GPU)) return 1;
                }
            }
        }
    }
    return 0;
}

_Bool PCIFindNvidiaGPU(NvidiaGPU *GPU) {
    if (!GPU) return 0;
    return PCIFindNvidiaGPU_Recursive(0, GPU);
}

u64 PCIGetNvidiaGPUBar64(u8 Bus, u8 Slot, u8 Func, u8 BARIndex){
    return PCIGetBARAddress(Bus, Slot, Func, BARIndex);
}

u64 PCIGetNvidiaGPUBarSize64(u8 Bus, u8 Slot, u8 Func, u8 BARIndex){
    return PCIGetBARSize64(Bus, Slot, Func, BARIndex);
}

_Bool NvidiaGPUInit(NvidiaGPU *GPU){
    if (!GPU) return 0;
    PCIEnableDevice(GPU->Bus, GPU->Slot, GPU->Func);
    GPU->Bar0Base=PCIGetBARAddress(GPU->Bus, GPU->Slot, GPU->Func, 0);
    DebugStr("BAR0 Base: ");
    DebugU64(GPU->Bar0Base);
    DebugChar('\n');
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
    GPU->VramSize=PCIGetNvidiaGPUBarSize64(GPU->Bus, GPU->Slot, GPU->Func, 1);
    GPU->VramBase=PCIGetNvidiaGPUBar64(GPU->Bus, GPU->Slot, GPU->Func, 1);
    return GPU->BootStatus==NVIDIA_BOOTED;
}
