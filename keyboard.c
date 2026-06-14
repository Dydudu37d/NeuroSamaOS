#include "keyboard.h"
#include "debug.h"
#include "kmalloc.h"
#include "str.h"

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

static void KeyboardQueueTransfer(XhciController* Xhci, KeyboardDevice* Kbd) {
    XhciDeviceContext* devCtx = (XhciDeviceContext*)Xhci->Dcbaa->DevCtxPtr[Kbd->SlotId];
    u32 epIdx = Kbd->EpIn;
    u64 deq = devCtx->Ep[epIdx - 1].DequeueLow | ((u64)devCtx->Ep[epIdx - 1].DequeueHigh << 32);
    XhciTrb* ring = (XhciTrb*)(deq & ~0xF);

    u8* buf = (u8*)AlignedAlloc(&KernelPool, Kbd->MaxPacketSize, 64);
    MemSet(buf, 0, Kbd->MaxPacketSize);

    ring[0].ParameterLow = (u32)((u64)buf & 0xFFFFFFFF);
    ring[0].ParameterHigh = (u32)((u64)buf >> 32);
    ring[0].Status = Kbd->MaxPacketSize;
    ring[0].Control = (XHCI_TRB_NORMAL << 10) | XHCI_TRB_IOC | 1;

    XhciRingEpDoorbell(Xhci, Kbd->SlotId, epIdx);
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
        if (!devCtx) {
            continue;
        }

        u8 desc[18];
        if (!UsbGetDescriptor(Xhci, slot, USB_DT_DEVICE, desc, 18)) {
            continue;
        }

        u8 cfgBuf[256];
        if (!UsbGetDescriptor(Xhci, slot, USB_DT_CONFIG, cfgBuf, 9)) {
            continue;
        }

        u16 totalLen = *(u16*)(cfgBuf + 2);
        if (!UsbGetDescriptor(Xhci, slot, USB_DT_CONFIG, cfgBuf, totalLen)) {
            continue;
        }

        u8 ep = 0;
        u16 maxPkt = 0;
        if (ParseConfig(cfgBuf, totalLen, &ep, &maxPkt)) {
            if (!UsbSetConfig(Xhci, slot, cfgBuf[5])) {
                continue;
            }

            if (!UsbSetIdle(Xhci, slot, 0, 0, 0)) {
                continue;
            }

            XhciInputContext* inputCtx = (XhciInputContext*)AlignedAlloc(&KernelPool, sizeof(XhciInputContext), 64);
            MemSet(inputCtx, 0, sizeof(XhciInputContext));
            inputCtx->InputCtrl.AddFlags = (1 << (ep + 1));
            
            XhciTrb* epRing = (XhciTrb*)AlignedAlloc(&KernelPool, sizeof(XhciTrb) * 64, 64);
            MemSet(epRing, 0, sizeof(XhciTrb) * 64);
            
            inputCtx->Ep[ep - 1].DequeueLow = (u32)((u64)epRing & 0xFFFFFFFF) | 1;
            inputCtx->Ep[ep - 1].DequeueHigh = (u32)((u64)epRing >> 32);
            inputCtx->Ep[ep - 1].EpInfo2 = (maxPkt << 16) | (3 << 3) | 3;

            XhciSendCommand(Xhci, (u64)inputCtx, 0, XHCI_TRB_CONFIGURE_ENDPOINT, slot);
            
            XhciTrb ev;
            while (1) {
                if (XhciReadEvent(Xhci, &ev)) {
                    u8 type = (ev.Control >> 10) & 0x3F;
                    if (type == XHCI_TRB_COMMAND_COMPLETE) {
                        break;
                    }
                }
            }

            KeyboardDevice* kbd = (KeyboardDevice*)Alloc(&KernelPool, sizeof(KeyboardDevice));
            MemSet(kbd, 0, sizeof(KeyboardDevice));
            kbd->SlotId = slot;
            kbd->EpIn = ep;
            kbd->MaxPacketSize = maxPkt;

            KeyboardQueueTransfer(Xhci, kbd);
            return kbd;
        }
    }
    return 0;
}

void KeyboardPoll(KeyboardDevice* Kbd) {
    if (!Kbd) return;

    XhciController* xhci = 0;
    XhciController* base = (XhciController*)0x10000;
    while (base->MmioBase != 0) {
        xhci = base;
        break;
    }
    if (!xhci) return;

    XhciTrb ev;
    if (XhciReadEvent(xhci, &ev)) {
        u8 type = (ev.Control >> 10) & 0x3F;
        if (type == XHCI_TRB_TRANSFER_EVENT) {
            u8 code = (ev.Status >> 24) & 0xFF;
            u8 slotId = (ev.Control >> 24) & 0xFF;
            
            if (slotId == Kbd->SlotId && code == XHCI_CMPLT_SUCCESS) {
                XhciDeviceContext* devCtx = (XhciDeviceContext*)xhci->Dcbaa->DevCtxPtr[Kbd->SlotId];
                u32 epIdx = Kbd->EpIn;
                u64 deq = devCtx->Ep[epIdx - 1].DequeueLow | ((u64)devCtx->Ep[epIdx - 1].DequeueHigh << 32);
                XhciTrb* ring = (XhciTrb*)(deq & ~0xF);
                u8* report = (u8*)((u64)ring[0].ParameterLow | ((u64)ring[0].ParameterHigh << 32));

                if (report && (report[2] || report[3] || report[4])) {
                    u8 scancode = report[2];
                    if (scancode >= 4 && scancode <= 0x65) {
                        const char map[] = "abcdefghijklmnopqrstuvwxyz1234567890\n";
                        u8 ascii = 0;
                        if (scancode >= 4 && scancode <= 38) {
                            ascii = map[scancode - 4];
                        } else if (scancode == 40) {
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
                Free(&KernelPool, report);
                KeyboardQueueTransfer(xhci, Kbd);
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
