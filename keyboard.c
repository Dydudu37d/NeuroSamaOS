#include "keyboard.h"
#include "debug.h"
#include "kmalloc.h"
#include "str.h"

const char map[] = "abcdefghijklmnopqrstuvwxyz1234567890\n";
extern AllocPool KernelPool;

void KeyboardQueueTransfer(XhciController* Xhci, KeyboardDevice* Kbd) {
    u32 idx = Kbd->Enqueue;
    u64 bufferPhys = (u64)Kbd->ReportBuffer;

    Kbd->KbdRing[idx].ParameterLow = (u32)(bufferPhys & 0xFFFFFFFF);
    Kbd->KbdRing[idx].ParameterHigh = (u32)(bufferPhys >> 32);
    Kbd->KbdRing[idx].Status = Kbd->MaxPacketSize;
    Kbd->KbdRing[idx].Control = (XHCI_TRB_NORMAL << 10) | XHCI_TRB_IOC | Kbd->Ccs;

    Kbd->Enqueue++;
    if (Kbd->Enqueue >= 63) {
        XhciTrb* link = &Kbd->KbdRing[Kbd->Enqueue];
        link->ParameterLow = (u32)((u64)Kbd->KbdRing & 0xFFFFFFFF);
        link->ParameterHigh = (u32)((u64)Kbd->KbdRing >> 32);
        link->Status = 0;
        link->Control = (XHCI_TRB_LINK << 10) | Kbd->Ccs;
        Kbd->Enqueue = 0;
        Kbd->Ccs ^= 1;
    }

    u8 dci = (Kbd->EpIn * 2) + 1;
    XhciRingDoorbell(Xhci, Kbd->SlotId, dci);
}

static u8 ParseConfig(u8* Cfg, u16 TotalLen, u8* OutIface, u8* OutEp, u16* OutMaxPkt) {
    u8* ptr = Cfg;
    u8* end = Cfg + TotalLen;
    u8 foundIface = 0;

    while (ptr < end) {
        u8 len = ptr[0];
        u8 type = ptr[1];

        if (type == USB_DT_INTERFACE) {
            u8 class = ptr[5];
            u8 sub = ptr[6];
            u8 proto = ptr[7];
            if (class == USB_CLASS_HID && sub == USB_SUBCLASS_BOOT && proto == USB_PROTOCOL_KEYBOARD) {
                foundIface = 1;
                if (OutIface) *OutIface = ptr[2];
            }
        }

        if (foundIface && type == USB_DT_ENDPOINT) {
            u8 addr = ptr[2];
            u8 attr = ptr[3];
            if ((attr & 0x03) == 3 && (addr & 0x80)) {
                *OutEp = addr & 0x0F;
                *OutMaxPkt = *(u16*)(ptr + 4);
                return 1;
            }
        }

        ptr += len;
    }
    return 0;
}

KeyboardDevice* KeyboardInit(XhciController* Xhci) {
    DebugStr("KeyboardInit: scanning slots\n");
    u8 maxSlots = Xhci->MaxSlots;

    for (u8 slot = 1; slot <= maxSlots; slot++) {
        XhciDeviceContext* devCtx = (XhciDeviceContext*)Xhci->Dcbaa->DevCtxPtr[slot];
        if (!devCtx) continue;

        UsbDeviceDescriptor devDesc;
        u8 status = XhciControlTransfer(Xhci, slot, 0x80, USB_REQ_GET_DESCRIPTOR,
                                 (USB_DT_DEVICE << 8) | 0, 0, sizeof(UsbDeviceDescriptor), &devDesc);
        DebugStr("Slot "); DebugU8(slot); DebugStr(" desc status: "); DebugU8(status); DebugStr("\n");
        if (status != XHCI_CMPLT_SUCCESS) continue;

        u8 cfgBuf[256];
        if (XhciControlTransfer(Xhci, slot, 0x80, USB_REQ_GET_DESCRIPTOR,
                                (USB_DT_CONFIG << 8) | 0, 0, 9, cfgBuf) != XHCI_CMPLT_SUCCESS)
            continue;
        u16 totalLen = *(u16*)(cfgBuf + 2);
        if (XhciControlTransfer(Xhci, slot, 0x80, USB_REQ_GET_DESCRIPTOR,
                                (USB_DT_CONFIG << 8) | 0, 0, totalLen, cfgBuf) != XHCI_CMPLT_SUCCESS)
            continue;

        u8 iface = 0, ep = 0;
        u16 maxPkt = 0;
        if (!ParseConfig(cfgBuf, totalLen, &iface, &ep, &maxPkt))
            continue;

        if (XhciControlTransfer(Xhci, slot, 0x00, USB_REQ_SET_CONFIG,
                                cfgBuf[5], 0, 0, NULL) != XHCI_CMPLT_SUCCESS)
            continue;

        if (XhciControlTransfer(Xhci, slot, 0x21, USB_REQ_SET_IDLE,
                                0, iface, 0, NULL) != XHCI_CMPLT_SUCCESS)
            continue;

        KeyboardDevice* kbd = (KeyboardDevice*)Alloc(&KernelPool, sizeof(KeyboardDevice));
        MemSet(kbd, 0, sizeof(KeyboardDevice));
        kbd->SlotId = slot;
        kbd->EpIn = ep;
        kbd->MaxPacketSize = maxPkt;
        kbd->Xhci = Xhci;
        kbd->Ccs = 1;
        kbd->Enqueue = 0;

        XhciInputContext* inputCtx = (XhciInputContext*)AlignedAlloc(&KernelPool, sizeof(XhciInputContext), 64);
        MemSet(inputCtx, 0, sizeof(XhciInputContext));
        u8 dci = (ep * 2) + 1;
        u8 epIdx = dci - 1;
        inputCtx->InputCtrl.AddFlags = (1 << dci);
        u64 ringPhys = (u64)kbd->KbdRing;
        inputCtx->Ep[epIdx].DequeueLow = (u32)(ringPhys & 0xFFFFFFFF) | 1;
        inputCtx->Ep[epIdx].DequeueHigh = (u32)(ringPhys >> 32);
        inputCtx->Ep[epIdx].EpInfo2 = (maxPkt << 16) | (3 << 3) | 3;
        XhciSendCommand(Xhci, (u64)inputCtx, 0, XHCI_TRB_CONFIGURE_ENDPOINT, slot);
        XhciTrb ev;
        while (1) {
            if (XhciReadEvent(Xhci, &ev) && ((ev.Control >> 10) & 0x3F) == XHCI_TRB_COMMAND_COMPLETE)
                break;
        }
        Free(&KernelPool, inputCtx);

        KeyboardQueueTransfer(Xhci, kbd);
        DebugStr("Keyboard initialized on slot ");
        DebugU8(slot);
        DebugStr("\n");
        return kbd;
    }
    DebugStr("Return NULL.\n");
    return NULL;
}

void KeyboardPushScancode(KeyboardDevice* Kbd, u8 Scancode) {
    if (!Kbd) return;
    if (Scancode >= 4 && Scancode <= 0x65) {
        u8 ascii = 0;
        if (Scancode >= 4 && Scancode <= 38)
            ascii = map[Scancode - 4];
        else if (Scancode == 40)
            ascii = '\n';
        if (ascii) {
            u8 next = (Kbd->WritePos + 1) % KEY_BUFFER_SIZE;
            if (next != Kbd->ReadPos) {
                Kbd->Buffer[Kbd->WritePos] = ascii;
                Kbd->WritePos = next;
            }
        }
    }
}

u8 KeyboardGetChar(KeyboardDevice* Kbd) {
    if (!Kbd || Kbd->ReadPos == Kbd->WritePos) return 0;
    u8 c = Kbd->Buffer[Kbd->ReadPos];
    Kbd->ReadPos = (Kbd->ReadPos + 1) % KEY_BUFFER_SIZE;
    return c;
}

u8 KeyboardHasChar(KeyboardDevice* Kbd) {
    return Kbd && Kbd->ReadPos != Kbd->WritePos;
}