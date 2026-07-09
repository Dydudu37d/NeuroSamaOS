#pragma once

#include "int.h"
#include "xhci.h"

typedef struct MouseDevice {
    u8 SlotId;
    u8 EpIn;
    u16 MaxPacketSize;
    u8 InterfaceNum;
    XhciTrb* TrbRing;
    u64 TrbRingPhys;
    u32 Enqueue;
    u8 Ccs;
    s32 X;
    s32 Y;
    u8 Buttons;
    s8 Buffer[4];
    u8 ReportBuffer[8];
    XhciController* Xhci;
} MouseDevice;

#define USB_DIR_IN             0x80
#define USB_DIR_OUT            0x00

MouseDevice* MouseInit(XhciController *Xhci);
void MousePoll(MouseDevice* Mouse);
void MouseGetDelta(MouseDevice* Mouse, s16* Dx, s16* Dy, u8* Buttons);
void MousePrepareTransfer(struct MouseDevice* Mouse);
