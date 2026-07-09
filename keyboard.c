#include "keyboard.h"
#include "debug.h"
#include "kmalloc.h"
#include "str.h"

const char map[] = "abcdefghijklmnopqrstuvwxyz1234567890\n";

extern AllocPool KernelPool;

static void XhciRingEpDoorbell(XhciController* Xhci, u8 SlotId, u8 EpId) {
    volatile u32* db = (volatile u32*)(Xhci->DoorbellBase + (SlotId * 4));
    *db = EpId;
}
static u8 UsbCtrlTransfer(XhciController* Xhci, u8 SlotId, u8 bmReqType, u8 bReq,
                          u16 wValue, u16 wIndex, u16 wLength, void* Data) {
    XhciDeviceContext* devCtx = (XhciDeviceContext*)Xhci->Dcbaa->DevCtxPtr[SlotId];
    u64 deq = devCtx->Ep[0].DequeueLow | ((u64)devCtx->Ep[0].DequeueHigh << 32);
    XhciTrb* ep0Ring = (XhciTrb*)(deq & ~0xF);

    u8 setup[8] = {
        bmReqType, bReq,
        (u8)(wValue & 0xFF), (u8)(wValue >> 8),
        (u8)(wIndex & 0xFF), (u8)(wIndex >> 8),
        (u8)(wLength & 0xFF), (u8)(wLength >> 8)
    };

    u8* setupBuf = (u8*)AlignedAlloc(&KernelPool, 8, 64);
    MemCopy(setupBuf, setup, 8);

    XhciTrb trbs[3];
    MemSet(trbs, 0, sizeof(trbs));

    trbs[0].ParameterLow = (u32)((u64)setupBuf & 0xFFFFFFFF);
    trbs[0].ParameterHigh = (u32)((u64)setupBuf >> 32);
    trbs[0].Status = 8;
    trbs[0].Control = (XHCI_TRB_SETUP << 10) | XHCI_TRB_IDT | 1;

    u8* dataBuf = 0;
    int dataIdx = -1;

    if (wLength && (bmReqType & USB_DIR_IN)) {
        dataBuf = (u8*)AlignedAlloc(&KernelPool, wLength, 64);
        trbs[1].ParameterLow = (u32)((u64)dataBuf & 0xFFFFFFFF);
        trbs[1].ParameterHigh = (u32)((u64)dataBuf >> 32);
        trbs[1].Status = wLength;
        trbs[1].Control = (XHCI_TRB_DATA << 10) | (1 << 16) | 1;
        dataIdx = 1;
    }

    int statusIdx = (dataIdx >= 0) ? 2 : 1;
    trbs[statusIdx].Control = (XHCI_TRB_STATUS << 10) | XHCI_TRB_IOC | 1;
    if (dataIdx < 0 && (bmReqType & USB_DIR_IN)) {
        trbs[statusIdx].Control |= (1 << 16);
    }

    for (int i = 0; i <= statusIdx; i++) {
        ep0Ring[i] = trbs[i];
    }

    XhciRingEpDoorbell(Xhci, SlotId, 1);

    u32 timeout = 100000;
    u8 success = 0;

    while (timeout--) {
        XhciTrb ev;
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            if (type == XHCI_TRB_TRANSFER_EVENT) {
                u8 code = (ev.Status >> 24) & 0xFF;
                if (code == XHCI_CMPLT_SUCCESS) {
                    success = 1;
                    break;
                }
            }
        }
        __asm__ volatile ("pause");
    }

    if (success && dataBuf && Data) {
        MemCopy(Data, dataBuf, wLength);
    }

    Free(&KernelPool, setupBuf);
    if (dataBuf) Free(&KernelPool, dataBuf);

    return success;
}

static u8 UsbGetDescriptor(XhciController* Xhci, u8 SlotId, u8 Type, void* Buf, u16 Len) {
    return UsbCtrlTransfer(Xhci, SlotId, USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                           USB_REQ_GET_DESCRIPTOR, (Type << 8), 0, Len, Buf);
}

static u8 UsbSetConfig(XhciController* Xhci, u8 SlotId, u8 Config) {
    return UsbCtrlTransfer(Xhci, SlotId, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                           USB_REQ_SET_CONFIG, Config, 0, 0, 0);
}

static u8 UsbSetIdle(XhciController* Xhci, u8 SlotId, u8 Iface, u8 Duration, u8 ReportId) {
    return UsbCtrlTransfer(Xhci, SlotId, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                           USB_REQ_SET_IDLE, (Duration << 8) | ReportId, Iface, 0, 0);
}

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
        link->Control = (XHCI_TRB_LINK << 10) | (1 << 1) | Kbd->Ccs;
        
        Kbd->Enqueue = 0;
        Kbd->Ccs ^= 1;
    }

    u8 dci = (Kbd->EpIn * 2) + 1;
    XhciRingEpDoorbell(Xhci, Kbd->SlotId, dci);
}

static u8 ParseConfig(u8* Cfg, u16 TotalLen, u8* OutEp, u16* OutMaxPkt) {
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

        u8 desc[18];
        if (!UsbGetDescriptor(Xhci, slot, USB_DT_DEVICE, desc, 18)) continue;

        u8 cfgBuf[256];
        if (!UsbGetDescriptor(Xhci, slot, USB_DT_CONFIG, cfgBuf, 9)) continue;

        u16 totalLen = *(u16*)(cfgBuf + 2);
        if (!UsbGetDescriptor(Xhci, slot, USB_DT_CONFIG, cfgBuf, totalLen)) continue;

        u8 ep = 0;
        u16 maxPkt = 0;
        if (ParseConfig(cfgBuf, totalLen, &ep, &maxPkt)) {
            if (!UsbSetConfig(Xhci, slot, cfgBuf[5])) continue;
            if (!UsbSetIdle(Xhci, slot, 0, 0, 0)) continue;

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
                if (XhciReadEvent(Xhci, &ev) && ((ev.Control >> 10) & 0x3F) == XHCI_TRB_COMMAND_COMPLETE) {
                    break;
                }
            }
            Free(&KernelPool, inputCtx);

            KeyboardQueueTransfer(Xhci, kbd);
            return kbd;
        }
    }
    return 0;
}

// ✨ 新增公有接口：提供給 xhci.c 的大總管直接分發掃描碼，徹底避免搶奪 EventRing
void KeyboardPushScancode(KeyboardDevice* Kbd, u8 Scancode) {
    if (!Kbd) return;

    if (Scancode >= 4 && Scancode <= 0x65) {
        u8 ascii = 0;
        if (Scancode >= 4 && Scancode <= 38) {
            ascii = map[Scancode - 4];
        } else if (Scancode == 40) {
            ascii = '\n';
        }

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