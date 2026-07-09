#pragma once

#include "int.h"
#include "xhci.h"

#define USB_CLASS_HID          0x03
#define USB_SUBCLASS_BOOT      0x01
#define USB_PROTOCOL_KEYBOARD  0x01
#define USB_PROTOCOL_MOUSE     0x02

#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_CONFIG     0x09
#define USB_REQ_SET_IDLE       0x0A

#define USB_DT_DEVICE          0x01
#define USB_DT_CONFIG          0x02
#define USB_DT_INTERFACE       0x04
#define USB_DT_ENDPOINT        0x05

#define USB_DIR_IN             0x80
#define USB_DIR_OUT            0x00
#define USB_TYPE_STANDARD      0x00
#define USB_TYPE_CLASS         0x20
#define USB_RECIP_DEVICE       0x00
#define USB_RECIP_INTERFACE    0x01

#define KEY_BUFFER_SIZE        16

typedef struct KeyboardDevice {
    u8 SlotId;
    u8 EpIn;
    u16 MaxPacketSize;
    u8 Buffer[KEY_BUFFER_SIZE];
    u8 ReadPos;
    u8 WritePos;
    u8 ReportBuffer[8];
    XhciTrb KbdRing[64];
    u32 Enqueue;
    u8 Ccs;
    XhciController* Xhci;
} KeyboardDevice;

void KeyboardPushScancode(struct KeyboardDevice* Kbd, u8 Scancode);
void KeyboardQueueTransfer(struct XhciController* Xhci, struct KeyboardDevice* Kbd);

KeyboardDevice* KeyboardInit(XhciController* Xhci);
u8 KeyboardGetChar(KeyboardDevice* Kbd);
u8 KeyboardHasChar(KeyboardDevice* Kbd);
