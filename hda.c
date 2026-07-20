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
    if (Slot == 0xFF) {
        DebugStr("HDAudio not found!\n");
        return 0;
    }
    return PCIGetBARAddress(Bus, Slot, Func, 0);
}

#pragma clang optimize off
HDAudio* HDAudioInit(){
    u64 HDAudioBase = HDAudioMMIO();
    if (!HDAudioBase){
        DebugStr("Cannot Found HDAudio.\n");
        return NULL;
    }else {
        DebugStr("HDAudio MMIO:");DebugU64(HDAudioBase);DebugChar('\n');
    }

    HDAudio* NewHDAudio = AlignedAlloc(&KernelPool, sizeof(HDAudio), 64);
    if (!NewHDAudio) {
        DebugStr("ERROR: Failed to allocate HDAudio struct\n");
        return NULL;
    }

    NewHDAudio->Bar0=HDAudioBase;
    NewHDAudio->Regs=(HDAudioRegs*)((u64*)NewHDAudio->Bar0);
    NewHDAudio->State=HDA_STATE_UNINITIALIZED;
    NewHDAudio->DmaAlloc.CorbWritePtr=&NewHDAudio->Regs->CORBWritePtr;
    NewHDAudio->DmaAlloc.RirbWritePtr=&NewHDAudio->Regs->RIRBWritePtr;
    NewHDAudio->CodecVendorId=0;
    NewHDAudio->CodecDeviceId=0;

    DebugStr("Debug: Print All Reg Offset:\n");
    DebugStr("GCAP Start:");DebugU64((u64)&NewHDAudio->Regs->GCAP);DebugChar(':');DebugChar('\n');
    DebugStr("\t{ GCAP:");DebugU16((u16)(((u64)&NewHDAudio->Regs->GCAP)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", VMIN:");DebugU16((u16)(((u64)&NewHDAudio->Regs->VMIN)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", VMAJ:");DebugU16((u16)(((u64)&NewHDAudio->Regs->VMAJ)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", OUTPAY:");DebugU16((u16)(((u64)&NewHDAudio->Regs->OUTPAY)-((u64)&NewHDAudio->Regs->GCAP)));DebugChar('\n');
    DebugStr("\t, INPAY:");DebugU16((u16)(((u64)&NewHDAudio->Regs->INPAY)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", GCTL:");DebugU16((u16)(((u64)&NewHDAudio->Regs->GCTL)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", WAKEEN:");DebugU16((u16)(((u64)&NewHDAudio->Regs->WAKEEN)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", STATESTS:");DebugU16((u16)(((u64)&NewHDAudio->Regs->STATESTS)-((u64)&NewHDAudio->Regs->GCAP)));DebugChar('\n');
    DebugStr("\t, GSTS:");DebugU16((u16)(((u64)&NewHDAudio->Regs->GSTS)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", OUTSTRMPAY:");DebugU16((u16)(((u64)&NewHDAudio->Regs->OUTSTRMPAY)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", INSTRMPAY:");DebugU16((u16)(((u64)&NewHDAudio->Regs->INSTRMPAY)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", INTCTL:");DebugU16((u16)(((u64)&NewHDAudio->Regs->INTCTL)-((u64)&NewHDAudio->Regs->GCAP)));DebugChar('\n');
    DebugStr("\t, INTSTS:");DebugU16((u16)(((u64)&NewHDAudio->Regs->INTSTS)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", WALCLK:");DebugU16((u16)(((u64)&NewHDAudio->Regs->WALCLK)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", SSYNC:");DebugU16((u16)(((u64)&NewHDAudio->Regs->SSYNC)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", CORBBase:");DebugU16((u16)(((u64)&NewHDAudio->Regs->CORBBase)-((u64)&NewHDAudio->Regs->GCAP)));DebugChar('\n');
    DebugStr("\t, CORBWritePtr:");DebugU16((u16)(((u64)&NewHDAudio->Regs->CORBWritePtr)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", CORBReadPtr:");DebugU16((u16)(((u64)&NewHDAudio->Regs->CORBReadPtr)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", CORBCTL:");DebugU16((u16)(((u64)&NewHDAudio->Regs->CORBCTL)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", CORBSTS:");DebugU16((u16)(((u64)&NewHDAudio->Regs->CORBSTS)-((u64)&NewHDAudio->Regs->GCAP)));DebugChar('\n');
    DebugStr("\t, CORBSize:");DebugU16((u16)(((u64)&NewHDAudio->Regs->CORBSize)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", RIRBBase:");DebugU16((u16)(((u64)&NewHDAudio->Regs->RIRBBase)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", RIRBWritePtr:");DebugU16((u16)(((u64)&NewHDAudio->Regs->RIRBWritePtr)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", RIRBCTL:");DebugU16((u16)(((u64)&NewHDAudio->Regs->RIRBCTL)-((u64)&NewHDAudio->Regs->GCAP)));DebugChar('\n');
    DebugStr("\t, RIRBSTS:");DebugU16((u16)(((u64)&NewHDAudio->Regs->RIRBSTS)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", RIRBSize:");DebugU16((u16)(((u64)&NewHDAudio->Regs->RIRBSize)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", ICOI:");DebugU16((u16)(((u64)&NewHDAudio->Regs->ICOI)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", ICII:");DebugU16((u16)(((u64)&NewHDAudio->Regs->ICII)-((u64)&NewHDAudio->Regs->GCAP)));DebugChar('\n');
    DebugStr("\t, ICIS:");DebugU16((u16)(((u64)&NewHDAudio->Regs->ICIS)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", DPBase:");DebugU16((u16)(((u64)&NewHDAudio->Regs->DPBase)-((u64)&NewHDAudio->Regs->GCAP)));;
    DebugStr(", Channels:");DebugU16((u16)(((u64)&NewHDAudio->Regs->Channels)-((u64)&NewHDAudio->Regs->GCAP)));
    DebugStr(", WALCLKA:");DebugU16((u16)(((u64)&NewHDAudio->Regs->WALCLKA)-((u64)&NewHDAudio->Regs->GCAP)));DebugChar('\n');
    DebugStr("\t, LPIBA:");DebugU16((u16)(((u64)&NewHDAudio->Regs->LPIBA)-((u64)&NewHDAudio->Regs->GCAP)));DebugChar(' ');DebugChar('}');
    DebugChar('\n');DebugChar('\n');

    DebugStr("Before Wake Up,GCTL.rwsControllerReset:");DebugU32(NewHDAudio->Regs->GCTL.rwsControllerReset);DebugChar('\n');
    NewHDAudio->Regs->GCTL.rwsControllerReset=0b0;
    NewHDAudio->Regs->GCTL.rwsControllerReset=0b1;
    while(!NewHDAudio->Regs->GCTL.rwsControllerReset) asm volatile("pause\n\t");
    DebugStr("After Wake Up, Wait 727us(727pp!WYSI(When you see it)!!!!!)\n");
    SystemBusySleepUs(727);
    DebugStr("After Wake Up,GCTL.rwsControllerReset:");DebugU32(NewHDAudio->Regs->GCTL.rwsControllerReset);DebugChar('\n');
    NewHDAudio->Regs->GCTL.rwFlushControl=1;
    while (NewHDAudio->Regs->GSTS.U & 0x01) {
        asm volatile("pause");
    }
    DebugStr("After Wake Up,Set 1 GCTL.rwFlushControl:");DebugU32(NewHDAudio->Regs->GCTL.rwFlushControl);DebugChar('\n');
    DebugStr("GCTL:");DebugU32(NewHDAudio->Regs->GCTL.U);DebugChar('\n');
    if (NewHDAudio->Regs->STATESTS.U == 0) NewHDAudio->Regs->STATESTS.U = 0xFFFF;
    DebugStr("STATESTS:");DebugU64Bit(NewHDAudio->Regs->STATESTS.U);DebugChar('\n');
    DebugStr("WAKEEN:");DebugU64Bit(NewHDAudio->Regs->WAKEEN.U);DebugChar('\n');
    DebugStr("GCAP:\n");
    DebugStr("\t.roOSSCount:");DebugU16(NewHDAudio->Regs->GCAP.roOSSCount);DebugChar('\n');
    DebugStr("\t.roISSCount:");DebugU16(NewHDAudio->Regs->GCAP.roISSCount);DebugChar('\n');
    DebugStr("\t.roBSSCount:");DebugU16(NewHDAudio->Regs->GCAP.roBSSCount);DebugChar('\n');
    DebugStr("\t.roSDOLineCount:");DebugU16(NewHDAudio->Regs->GCAP.roSDOLineCount);DebugChar('\n');
    DebugStr("\t.roHave64bit:");DebugU16(NewHDAudio->Regs->GCAP.roHave64bit);DebugChar('\n');
    DebugStr("Version(JMAJ.VMIN): ");DebugU8(NewHDAudio->Regs->VMAJ.U);DebugChar('.');DebugU8(NewHDAudio->Regs->VMIN.U);DebugChar('\n');

    NewHDAudio->NumStreams=NewHDAudio->Regs->GCAP.roSDOLineCount;

    NewHDAudio->Regs->CORBCTL.rwEnableDMAEngine=0b0;
    NewHDAudio->Regs->RIRBCTL.rwDMAEnable=0b0;

    NewHDAudio->NumCORB=
        NewHDAudio->Regs->CORBSize.roSize==CORB_2Entry ? 2 :
        NewHDAudio->Regs->CORBSize.roSize==CORB_16Entry ? 16 :
        NewHDAudio->Regs->CORBSize.roSize==CORB_256Entry ? 256 :
        CORB_NoWayBro_Its_RESERVED_And_ERROR
    ;

    NewHDAudio->NumRIRB=
        NewHDAudio->Regs->RIRBSize.roSize==CORB_2Entry ? 2 :
        NewHDAudio->Regs->RIRBSize.roSize==CORB_16Entry ? 16 :
        NewHDAudio->Regs->RIRBSize.roSize==CORB_256Entry ? 256 :
        CORB_NoWayBro_Its_RESERVED_And_ERROR
    ;

    DebugStr("Num CORB:");DebugU16(NewHDAudio->NumCORB);DebugChar('\n');
    DebugStr("Num RIRB:");DebugU16(NewHDAudio->NumRIRB);DebugChar('\n');

    NewHDAudio->DmaAlloc.Corb=AlignedAlloc(&KernelPool,NewHDAudio->NumCORB*sizeof(HDAudioCorbEntry),128);
    NewHDAudio->DmaAlloc.Rirb=AlignedAlloc(&KernelPool,NewHDAudio->NumRIRB*sizeof(HDAudioRirbEntry),128);
    u64 DPBase=(u64)AlignedAlloc(&KernelPool,4096,128);
    NewHDAudio->Regs->DPBase.U=DPBase;
    NewHDAudio->Regs->DPBase.rsvdzRsvd1=0;
    NewHDAudio->Regs->DPBase.rwPositionBufferEnable=0b1;

    DebugStr("Alloc CORB:");DebugU64((u64)NewHDAudio->DmaAlloc.Corb);DebugChar('\n');
    DebugStr("Alloc RIRB:");DebugU64((u64)NewHDAudio->DmaAlloc.Rirb);DebugChar('\n');
    DebugStr("Alloc DP:");DebugU64(((u64)NewHDAudio->Regs->DPBase.rwHighBase<<32)|((u64)NewHDAudio->Regs->DPBase.rwLowBase<<7));DebugChar('\n');

    NewHDAudio->Regs->CORBBase.U=(u64)NewHDAudio->DmaAlloc.Corb;
    NewHDAudio->Regs->RIRBBase.U=(u64)NewHDAudio->DmaAlloc.Rirb;

    NewHDAudio->Regs->CORBCTL.rwMemoryErrorInterruptEnable=0b0;
    NewHDAudio->Regs->RIRBCTL.rwResponseInterruptControl=0b0;

    NewHDAudio->Regs->RIRBWritePtr.rwWritePtrReset=0b1;
    SystemBusySleepUs(10);
    NewHDAudio->Regs->RIRBWritePtr.rwWritePtrReset=0b0;

    u32 WaitTimeout = 100000;
    while (NewHDAudio->Regs->RIRBWritePtr.roWritePtr != 0 && --WaitTimeout) {
        asm volatile("pause");
    }
    if (WaitTimeout == 0) {
        DebugStr("RIRB reset Timeout, WP="); DebugU16(NewHDAudio->Regs->RIRBWritePtr.roWritePtr); DebugChar('\n');
    }

    for (u16 i = 0; i < NewHDAudio->NumRIRB; i++) {
        NewHDAudio->DmaAlloc.Rirb[i].U = 0;
    }
    asm volatile("sfence" ::: "memory");

    DebugStr("After reset, RIRBWP: "); DebugU16(NewHDAudio->Regs->RIRBWritePtr.roWritePtr); DebugChar('\n');

    NewHDAudio->Regs->CORBReadPtr.rwReset=0b1;
    while (!(NewHDAudio->Regs->CORBReadPtr.U & 0x8000)) {
        asm volatile("pause");
    }
    NewHDAudio->Regs->CORBReadPtr.rwReset=0b0;
    while (NewHDAudio->Regs->CORBReadPtr.U & 0x8000) {
        asm volatile("pause");
    }
    NewHDAudio->Regs->CORBWritePtr.rwWritePtr=0;

    DebugStr("CORBBase:");DebugU64(NewHDAudio->Regs->CORBBase.U);DebugChar('\n');
    DebugStr("RIRBBase:");DebugU64(NewHDAudio->Regs->RIRBBase.U);DebugChar('\n');
    DebugStr("DPBase:");DebugU64(((u64)NewHDAudio->Regs->DPBase.rwHighBase<<32)|((u64)NewHDAudio->Regs->DPBase.rwLowBase<<7));DebugChar('\n');

    DebugStr("RIRBWP: ");DebugU16(NewHDAudio->Regs->RIRBWritePtr.roWritePtr);DebugChar('\n');

    for (u16 i = 0; i < NewHDAudio->NumRIRB; i++) {
        NewHDAudio->DmaAlloc.Rirb[i].U = 0;
    }
    asm volatile("sfence" ::: "memory");
    NewHDAudio->Regs->RIRBCTL.U = 0;
    NewHDAudio->Regs->CORBCTL.U = 0;

    NewHDAudio->Regs->RINTCNT.rwResponseInterruptCount=0b00000001;

    NewHDAudio->Regs->RIRBCTL.rwDMAEnable=1;
    NewHDAudio->Regs->RIRBCTL.rwResponseInterruptControl=1;
    NewHDAudio->Regs->CORBCTL.rwEnableDMAEngine=1;

    DebugStr("DMA+INT Enabled - CORBCTL:"); DebugU8(NewHDAudio->Regs->CORBCTL.U);
    DebugStr(" RIRBCTL:"); DebugU8(NewHDAudio->Regs->RIRBCTL.U); DebugChar('\n');

    NewHDAudio->Regs->RIRBSTS.U = 0xFF;

    u16 StateSTS = NewHDAudio->Regs->STATESTS.U & 0xFFFF;
    u8 ValidCodecAddr = 0;
    for (u8 i = 0; i < 16; i++) {
        if (StateSTS & (1 << i)) {
            ValidCodecAddr = i;
            break;
        }
    }
    DebugStr("Valid Codec: "); DebugU8(ValidCodecAddr); DebugChar('\n');

    if (NewHDAudio->Regs->RIRBWritePtr.roWritePtr != 0) {
        DebugStr("Dropping existing RIRB response, WP="); 
        DebugU16(NewHDAudio->Regs->RIRBWritePtr.roWritePtr); 
        DebugChar('\n');
        u16 DropIdx = (NewHDAudio->Regs->RIRBWritePtr.roWritePtr - 1) & (NewHDAudio->NumRIRB - 1);
        NewHDAudio->DmaAlloc.Rirb[DropIdx].U = 0;
        NewHDAudio->Regs->RIRBWritePtr.rwWritePtrReset = 1;
        SystemBusySleepUs(10);
        NewHDAudio->Regs->RIRBWritePtr.rwWritePtrReset = 0;
        NewHDAudio->Regs->RIRBSTS.U = 0xFF;
        DebugStr("After clearing, RIRBWP: "); 
        DebugU16(NewHDAudio->Regs->RIRBWritePtr.roWritePtr); 
        DebugChar('\n');
    }

    SystemBusySleepUs(100);

    NewHDAudio->DmaAlloc.Corb[1].U = 0;
    NewHDAudio->DmaAlloc.Corb[1].rwCodecAddr = ValidCodecAddr;
    NewHDAudio->DmaAlloc.Corb[1].rwNodeId = 0;
    NewHDAudio->DmaAlloc.Corb[1].rwVerbId = 0xF00;
    NewHDAudio->DmaAlloc.Corb[1].rwParameter = 0x00;

    asm volatile("sfence" ::: "memory");

    NewHDAudio->Regs->CORBWritePtr.rwWritePtr = 1;

    DebugStr("Send Corb\n");
    DebugStr("CORBWP before wait: "); DebugU16(NewHDAudio->Regs->CORBWritePtr.rwWritePtr); DebugChar('\n');
    DebugStr("RIRBWP before wait: "); DebugU16(NewHDAudio->Regs->RIRBWritePtr.roWritePtr); DebugChar('\n');
    DebugStr("CORBCTL: "); DebugU8(NewHDAudio->Regs->CORBCTL.U); DebugChar('\n');
    DebugStr("RIRBCTL: "); DebugU8(NewHDAudio->Regs->RIRBCTL.U); DebugChar('\n');
    DebugStr("CORBSTS: "); DebugU8(NewHDAudio->Regs->CORBSTS.U); DebugChar('\n');
    DebugStr("RIRBSTS: "); DebugU8(NewHDAudio->Regs->RIRBSTS.U); DebugChar('\n');
    DebugStr("RINTCNT: "); DebugU8(NewHDAudio->Regs->RINTCNT.U); DebugChar('\n');
    DebugStr("CORBWP full: ");
    DebugU16(*(volatile u16*)((u64)NewHDAudio->Bar0 + 0x48));
    DebugChar('\n');
    DebugStr("CORBRP full: ");
    DebugU16(*(volatile u16*)((u64)NewHDAudio->Bar0 + 0x4A));
    DebugChar('\n');

    u16 OldRirb = NewHDAudio->Regs->RIRBWritePtr.roWritePtr;
    u32 Timeout = 1000000;
    while (OldRirb == NewHDAudio->Regs->RIRBWritePtr.roWritePtr){
        asm volatile("pause\n\t");
        if (--Timeout == 0) {
            DebugStr("TIMEOUT! RIRBCTL="); DebugU8(NewHDAudio->Regs->RIRBCTL.U);
            DebugStr(" RIRBSTS="); DebugU8(NewHDAudio->Regs->RIRBSTS.U);
            DebugStr(" CORBRP="); DebugU16(NewHDAudio->Regs->CORBReadPtr.roReadPtr);
            DebugChar('\n');
            break;
        }
    }

    if (Timeout > 0) {
        u16 RirbWp = NewHDAudio->Regs->RIRBWritePtr.roWritePtr;
        u16 idx = (RirbWp == 0) ? (NewHDAudio->NumRIRB - 1) : (RirbWp - 1);
        DebugStr("Get RIRB\n");
        DebugStr("RIRB Get:");DebugU64(NewHDAudio->DmaAlloc.Rirb[idx].U);DebugChar('\n');
    } else {
        DebugStr("Failed to receive RIRB response.\n");
    }

    return NewHDAudio;
}
#pragma clang optimize on