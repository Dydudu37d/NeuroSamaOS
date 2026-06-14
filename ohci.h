#pragma once

#include "int.h"

typedef struct OhciRegs {
    volatile u32 HcControl;
    volatile u32 HcCommandStatus;
    volatile u32 HcInterruptStatus;
    volatile u32 HcInterruptEnable;
    volatile u32 HcInterruptDisable;
    volatile u32 HcHCCA;
    volatile u32 HcPeriodCurrentED;
    volatile u32 HcControlHeadED;
    volatile u32 HcControlCurrentED;
    volatile u32 HcBulkHeadED;
    volatile u32 HcBulkCurrentED;
    volatile u32 HcDoneHead;
    volatile u32 HcFmInterval;
    volatile u32 HcFmRemaining;
    volatile u32 HcFmNumber;
    volatile u32 HcPeriodicStart;
    volatile u32 HcLSThreshold;
    volatile u32 HcRhDescriptorA;
    volatile u32 HcRhDescriptorB;
    volatile u32 HcRhStatus;
    volatile u32 HcRhPortStatus[15];
} __attribute__((packed)) OhciRegs;

typedef struct OhciHcca {
    volatile u32 HccaInterruptTable[32];
    volatile u16 HccaFrameNumber;
    volatile u16 HccaPad1;
    volatile u32 HccaDoneHead;
    volatile u8 HccaReserved[116];
} __attribute__((packed)) OhciHcca;

typedef struct OhciEd {
    volatile u32 Control;
    volatile u32 Tail;
    volatile u32 Head;
    volatile u32 Next;
} __attribute__((packed)) OhciEd;

typedef struct OhciTd {
    volatile u32 Control;
    volatile u32 CurrentBuffer;
    volatile u32 Next;
    volatile u32 BufferEnd;
} __attribute__((packed)) OhciTd;

typedef struct OhciController {
    u64 MmioBase;
    OhciRegs* Regs;
    OhciHcca* Hcca;
    OhciEd* ControlEd;
    OhciEd* BulkEd;
    OhciEd* KbdEd;
    OhciTd* KbdTd;
    u8* KbdBuffer;
    u8 HaveKeyboard;
    u8 KbdAddress;
    u8 LastChar;
} OhciController;

OhciController* OhciInit(u64 MmioBase);
void OhciPollEvents(OhciController* ohci);
u8 OhciGetChar(OhciController* ohci);
