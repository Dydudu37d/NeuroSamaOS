#include "hda.h"
#include "int.h"
#include "pci.h"
#include "kmalloc.h"
#include "debug.h"
#include "clock.h"

extern AllocPool KernelPool;

u64 HDAudioMMIO(){
    u8 Bus,Slot,Func;
    PCIFindDeviceByClass(0x04, 0x03, 0x00, &Bus, &Slot, &Func);
    return Slot!=0xFF ? PCIGetBARAddress(Bus, Slot, Func, 0) : 0;
}

HDAudio* HDAudioInit(){
    u64 HDAudioBase = HDAudioMMIO();
    if (!HDAudioBase){
        DebugStr("Cannot Found HDAudio.\n");
        return NULL;
    }
    HDAudio* NewHDAudio = AlignedAlloc(&KernelPool, sizeof(HDAudio), 64);
    NewHDAudio->Bar0 = HDAudioBase;
    NewHDAudio->Regs = (HDAudioRegs*)(u64*)HDAudioBase;
    DebugStr("Read GCTL:");DebugU64(NewHDAudio->Regs->GCTL.U);DebugChar('\n');
    DebugStr("Write CRST to 0(Reset).\n");
    NewHDAudio->Regs->GCTL.rwsControllerReset=0;
    while (NewHDAudio->Regs->GCTL.rwsControllerReset!=0) {
        asm volatile("pause\n\t");
        DebugStr("Loop.\n");
    }
    DebugStr("Write CRST to 1(Go Work!).\n");
    NewHDAudio->Regs->GCTL.rwsControllerReset=1;
    while (NewHDAudio->Regs->GCTL.rwsControllerReset!=1) {
        asm volatile("pause\n\t");
        DebugStr("Loop.\n");
    }
    DebugStr("Wait 1000Ms.\n");
    SystemBusySleepMs(1000);
    DebugStr("Read STATESTS:");
    DebugU64Bit((u64)NewHDAudio->Regs->STATESTS.rw1csSerialDataIn);
    DebugChar('\n');

    return NewHDAudio;
}