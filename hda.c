#include "hda.h"
#include "int.h"
#include "pci.h"
#include "kmalloc.h"
#include "debug.h"
#include "clock.h"

extern AllocPool KernelPool;

u64 HDAudioMMIO(){
    u8 Bus,Slot,Func;
    PCIFindDeviceByClassList((u8[3]){0x04,0x04,0x04}, (u8[3]){0x03,0x03,0x01}, (u8[3]){0x00,0x80,0x00}, 3, &Bus, &Slot, &Func);
    return Slot!=0xFF ? PCIGetBARAddress(Bus, Slot, Func, 0) : 0;
}

HDAudio* HDAudioInit(){
    u64 HDAudioBase = HDAudioMMIO();
    if (!HDAudioBase){
        DebugStr("Cannot Found HDAudio.\n");
        return NULL;
    }else {
        DebugStr("HDAudio MMIO:");DebugU64(HDAudioBase);DebugChar('\n');
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
        if (NewHDAudio->Regs->GCTL.rwsControllerReset!=0) NewHDAudio->Regs->GCTL.rwsControllerReset=0;
    }
    DebugStr("Write CRST to 1(Go Work!).\n");
    NewHDAudio->Regs->GCTL.rwsControllerReset=1;
    while (NewHDAudio->Regs->GCTL.rwsControllerReset!=1) {
        asm volatile("pause\n\t");
        DebugStr("Loop.\n");
        if (NewHDAudio->Regs->GCTL.rwsControllerReset!=1) NewHDAudio->Regs->GCTL.rwsControllerReset=1;
    }
    NewHDAudio->Regs->STATESTS.U = 0xFFFF;
    NewHDAudio->Regs->STATESTS.rsvdzRsvd1 = 0b0;
    DebugStr("Wait 1000Ms.\n");
    SystemBusySleepMs(1000);
    DebugStr("Read STATESTS:");
    DebugU64Bit((u64)NewHDAudio->Regs->STATESTS.rw1csSerialDataIn);
    NewHDAudio->NumStreams=NewHDAudio->Regs->STATESTS.rw1csSerialDataIn;
    DebugChar('\n');
    DebugStr("STATESTS offset = 0x");
    DebugU64((u64)&NewHDAudio->Regs->STATESTS - (u64)NewHDAudio->Regs);
    DebugChar('\n');
    DebugStr("GCAP = 0x"); DebugU64(NewHDAudio->Regs->GCAP.U); DebugChar('\n');
    DebugStr("Stop CORB Run.\n");
    NewHDAudio->Regs->CORBCTL.rwEnableDMAEngine = 0b0;
    while (NewHDAudio->Regs->CORBCTL.rwEnableDMAEngine!=0) {
        asm volatile("pause\n\t");
        DebugStr("Loop.\n");
        if (NewHDAudio->Regs->CORBCTL.rwEnableDMAEngine!=0) NewHDAudio->Regs->CORBCTL.rwEnableDMAEngine=0;
    }
    DebugStr("Get CORBSIZE.\n");
    DebugU8(NewHDAudio->Regs->CORBSize.roSize);
    switch (NewHDAudio->Regs->CORBSize.roSize){
        case CORB_2Entry:
            DebugStr("2Entry.\n");
            NewHDAudio->NumCORB=2;
            break;
        case CORB_16Entry:
            DebugStr("16Entry.\n");
            NewHDAudio->NumCORB=16;
            break;
        case CORB_256Entry:
            DebugStr("256Entry.\n");
            NewHDAudio->NumCORB=256;
            break;
        case CORB_NoWayBro_Its_RESERVED_And_ERROR:
            DebugStr("No Way Bro,Its Error,Maybe Need To reset.\n");
            break;
        default:
            DebugStr("No Way Bro,Its Error,Maybe Need To reset.\n");
            break;
    }
    NewHDAudio->DmaAlloc.Corb=AlignedAlloc(&KernelPool, NewHDAudio->NumCORB*sizeof(HDAudioCorbEntry), 128);
    NewHDAudio->DmaAlloc.CorbWritePtr=NewHDAudio->Regs->CORBWritePtr;
    
    void* DPosBase = AlignedAlloc(&KernelPool, 4096, 128);
    if (!DPosBase) {
        DebugStr("Failed to allocate Position Buffer!\n");
        return NULL;
    }
    NewHDAudio->Regs->DPBase.rwLowBase = ((u64)DPosBase >> 7) & 0x1FFFFFF;
    NewHDAudio->Regs->DPBase.rsvdzRsvd1 = 0;
    NewHDAudio->Regs->DPBase.rwPositionBufferEnable = 1;
    NewHDAudio->Regs->DPBase.rwHighBase = ((u64)DPosBase >> 32) & 0xFFFFFFFF;


    return NewHDAudio;
}