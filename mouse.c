#include "mouse.h"
#include "debug.h"
#include "kmalloc.h"
#include "str.h"

extern AllocPool KernelPool;
static MouseDevice* ActiveMouse = 0;

static void XhciRingEpDoorbell(XhciController* Xhci, u8 SlotId, u8 EpId) {
    volatile u32* db = (volatile u32*)(Xhci->DoorbellBase + (SlotId * 4));
    *db = EpId;
}

static u8 UsbControlTransferMouse(XhciController* Xhci, u8 SlotId, u8 EpId,
                                  u8 bmRequestType, u8 bRequest, u16 wValue,
                                  u16 wIndex, u16 wLength, void* Buffer) {
    XhciDeviceContext* devCtx = (XhciDeviceContext*)Xhci->Dcbaa->DevCtxPtr[SlotId];
    u64 deq = devCtx->Ep[0].DequeueLow | ((u64)devCtx->Ep[0].DequeueHigh << 32);
    XhciTrb* ep0Ring = (XhciTrb*)(deq & ~0xF);

    u8 setupData[8] = {
        bmRequestType, bRequest,
        (u8)(wValue & 0xFF), (u8)(wValue >> 8),
        (u8)(wIndex & 0xFF), (u8)(wIndex >> 8),
        (u8)(wLength & 0xFF), (u8)(wLength >> 8)
    };

    u8* setupBuf = (u8*)AlignedAlloc(&KernelPool, 8, 64);
    MemCopy(setupBuf, setupData, 8);

    XhciTrb trbs[3];
    MemSet(trbs, 0, sizeof(trbs));

    trbs[0].ParameterLow = (u32)((u64)setupBuf & 0xFFFFFFFF);
    trbs[0].ParameterHigh = (u32)((u64)setupBuf >> 32);
    trbs[0].Status = 8;
    trbs[0].Control = (XHCI_TRB_SETUP << 10) | XHCI_TRB_IDT | 1;

    u8* dataBuf = 0;
    int dataTrbIndex = -1;

    if (wLength && (bmRequestType & USB_DIR_IN)) {
        dataBuf = (u8*)AlignedAlloc(&KernelPool, wLength, 64);
        trbs[1].ParameterLow = (u32)((u64)dataBuf & 0xFFFFFFFF);
        trbs[1].ParameterHigh = (u32)((u64)dataBuf >> 32);
        trbs[1].Status = wLength;
        trbs[1].Control = (XHCI_TRB_DATA << 10) | (1 << 16) | 1;
        dataTrbIndex = 1;
    }

    int statusIndex = (dataTrbIndex >= 0) ? 2 : 1;
    trbs[statusIndex].ParameterLow = 0;
    trbs[statusIndex].ParameterHigh = 0;
    trbs[statusIndex].Status = 0;
    trbs[statusIndex].Control = (XHCI_TRB_STATUS << 10) | XHCI_TRB_IOC | 1;
    if (dataTrbIndex < 0 && (bmRequestType & USB_DIR_IN)) {
        trbs[statusIndex].Control |= (1 << 16);
    }

    for (int i = 0; i <= statusIndex; i++) {
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
                u8 slot = (ev.Control >> 24) & 0xFF;
                if (slot == SlotId) {
                    u8 code = (ev.Status >> 24) & 0xFF;
                    if (code == XHCI_CMPLT_SUCCESS) {
                        success = 1;
                        break;
                    }
                }
            }
        }
        __asm__ volatile ("pause");
    }

    if (success && dataBuf && Buffer) {
        MemCopy(Buffer, dataBuf, wLength);
    }

    Free(&KernelPool, setupBuf);
    if (dataBuf) Free(&KernelPool, dataBuf);
    return success;
}

static void MouseSetupEndpoint(XhciController* Xhci, MouseDevice* Mouse) {
    size_t ringSize = sizeof(XhciTrb) * 64;
    Mouse->TrbRing = (XhciTrb*)AlignedAlloc(&KernelPool, ringSize, 64);
    MemSet(Mouse->TrbRing, 0, ringSize);

    Mouse->TrbRingPhys = (u64)Mouse->TrbRing;
    Mouse->Ccs = 1;
    Mouse->Enqueue = 0;

    XhciInputContext* inputCtx = (XhciInputContext*)AlignedAlloc(&KernelPool, sizeof(XhciInputContext), 64);
    MemSet(inputCtx, 0, sizeof(XhciInputContext));

    u8 ep = Mouse->EpIn;
    inputCtx->InputCtrl.AddFlags = (1 << (ep + 1));

    inputCtx->Ep[ep - 1].DequeueLow = (u32)(Mouse->TrbRingPhys & 0xFFFFFFFF) | 1;
    inputCtx->Ep[ep - 1].DequeueHigh = (u32)(Mouse->TrbRingPhys >> 32);
    inputCtx->Ep[ep - 1].EpInfo2 = (Mouse->MaxPacketSize << 16) | (3 << 3) | 3;

    XhciSendCommand(Xhci, (u64)inputCtx, 0, XHCI_TRB_CONFIGURE_ENDPOINT, Mouse->SlotId);

    XhciTrb ev;
    while (1) {
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            if (type == XHCI_TRB_COMMAND_COMPLETE) {
                break;
            }
        }
    }

    Free(&KernelPool, inputCtx);
}

static void MousePrepareTransfer(MouseDevice* Mouse) {
    u32 idx = Mouse->Enqueue;
    u8* buffer = (u8*)AlignedAlloc(&KernelPool, Mouse->MaxPacketSize, 64);
    MemSet(buffer, 0, Mouse->MaxPacketSize);

    Mouse->TrbRing[idx].ParameterLow = (u32)((u64)buffer & 0xFFFFFFFF);
    Mouse->TrbRing[idx].ParameterHigh = (u32)((u64)buffer >> 32);
    Mouse->TrbRing[idx].Status = Mouse->MaxPacketSize;
    Mouse->TrbRing[idx].Control = (XHCI_TRB_NORMAL << 10) | XHCI_TRB_IOC | Mouse->Ccs;

    Mouse->Enqueue++;
    if (Mouse->Enqueue >= 63) {
        XhciTrb* link = &Mouse->TrbRing[Mouse->Enqueue];
        link->ParameterLow = (u32)(Mouse->TrbRingPhys & 0xFFFFFFFF);
        link->ParameterHigh = (u32)(Mouse->TrbRingPhys >> 32);
        link->Status = 0;
        link->Control = (XHCI_TRB_LINK << 10) | (1 << 1) | Mouse->Ccs;
        
        Mouse->Enqueue = 0;
        Mouse->Ccs ^= 1;
    }
}

MouseDevice* MouseInit(XhciController *Xhci){
    u32 hcsparams1 = Xhci->Cap->Hcsparams1;
    u8 maxSlots = hcsparams1 & 0xFF;

    for (u8 slot = 1; slot <= maxSlots; slot++) {
        XhciDeviceContext* devCtx = (XhciDeviceContext*)Xhci->Dcbaa->DevCtxPtr[slot];
        if (!devCtx) continue;

        u8 deviceDescBuf[18];
        if (!UsbControlTransferMouse(Xhci, slot, 1, 0x80, 6, 0x0100, 0, 18, deviceDescBuf)) {
            continue;
        }

        UsbDeviceDescriptor* deviceDesc = (UsbDeviceDescriptor*)deviceDescBuf;
        if (deviceDesc->bDeviceClass == 0 && deviceDesc->bNumConfigurations > 0) {
            u8 configDescBuf[256];
            if (!UsbControlTransferMouse(Xhci, slot, 1, 0x80, 6, 0x0200, 0, 9, configDescBuf)) {
                continue;
            }

            u16 totalLength = *(u16*)(configDescBuf + 2);
            if (!UsbControlTransferMouse(Xhci, slot, 1, 0x80, 6, 0x0200, 0, totalLength, configDescBuf)) {
                continue;
            }

            u8* ptr = configDescBuf;
            u8* end = configDescBuf + totalLength;
            u8 found = 0;
            u8 ifaceNum = 0, epIn = 0;
            u16 maxPacketSize = 0;

            while (ptr < end && !found) {
                u8 len = ptr[0];
                u8 type = ptr[1];

                if (type == 0x04) {
                    UsbInterfaceDescriptor* iface = (UsbInterfaceDescriptor*)ptr;
                    if (iface->bInterfaceClass == 0x03 &&
                        iface->bInterfaceSubClass == 0x01 &&
                        iface->bInterfaceProtocol == 0x02) {
                        ifaceNum = iface->bInterfaceNumber;
                        found = 1;
                    }
                }

                if (found && type == 0x05) {
                    UsbEndpointDescriptor* ep = (UsbEndpointDescriptor*)ptr;
                    if ((ep->bmAttributes & 0x03) == 0x03) {
                        if (ep->bEndpointAddress & 0x80) {
                            epIn = ep->bEndpointAddress & 0x0F;
                            maxPacketSize = ep->wMaxPacketSize;
                            break;
                        }
                    }
                }

                ptr += len;
            }

            if (found && epIn) {
                if (!UsbControlTransferMouse(Xhci, slot, 1, 0x00, 9, deviceDesc->bNumConfigurations, 0, 0, 0)) {
                    continue;
                }

                MouseDevice* mouse = (MouseDevice*)Alloc(&KernelPool, sizeof(MouseDevice));
                MemSet(mouse, 0, sizeof(MouseDevice));

                mouse->SlotId = slot;
                mouse->EpIn = epIn;
                mouse->MaxPacketSize = maxPacketSize;
                mouse->InterfaceNum = ifaceNum;
                mouse->Xhci = Xhci;

                MouseSetupEndpoint(Xhci, mouse);
                MousePrepareTransfer(mouse);

                XhciRingEpDoorbell(Xhci, slot, epIn);

                ActiveMouse = mouse;
                DebugStr("MOUSE INIT DONE\n");
                return mouse;
            }
        }
    }
}

void MousePoll(MouseDevice* Mouse) {
    if (!Mouse) return;

    XhciController* xhci = Mouse->Xhci;
    if (!xhci) return;

    XhciTrb ev;
    if (XhciReadEvent(xhci, &ev)) {
        u8 type = (ev.Control >> 10) & 0x3F;
        if (type == XHCI_TRB_TRANSFER_EVENT) {
            u8 code = (ev.Status >> 24) & 0xFF;
            u8 slotId = (ev.Control >> 24) & 0xFF;
            
            if (slotId == Mouse->SlotId && code == XHCI_CMPLT_SUCCESS) {
                XhciDeviceContext* devCtx = (XhciDeviceContext*)xhci->Dcbaa->DevCtxPtr[Mouse->SlotId];
                u32 epIdx = Mouse->EpIn;
                u64 deq = devCtx->Ep[epIdx - 1].DequeueLow | ((u64)devCtx->Ep[epIdx - 1].DequeueHigh << 32);
                XhciTrb* ring = (XhciTrb*)(deq & ~0xF);
                
                u8* report = (u8*)((u64)ring[0].ParameterLow | ((u64)ring[0].ParameterHigh << 32));
                if (report) {
                    Mouse->Buttons = report[0] & 0x07;
                    s8 dx = (s8)report[1];
                    s8 dy = (s8)report[2];
                    Mouse->X += dx;
                    Mouse->Y += dy;

                    Mouse->Buffer[0] = dx;
                    Mouse->Buffer[1] = dy;
                    Mouse->Buffer[2] = report[0] & 0x07;
                    Mouse->Buffer[3] = 0;

                    Free(&KernelPool, report);
                }
                MousePrepareTransfer(Mouse);
                XhciRingEpDoorbell(xhci, Mouse->SlotId, Mouse->EpIn);
            }
        }
    }
}

void MouseGetDelta(MouseDevice* Mouse, s16* Dx, s16* Dy, u8* Buttons) {
    if (!Mouse) {
        if (Dx) *Dx = 0;
        if (Dy) *Dy = 0;
        if (Buttons) *Buttons = 0;
        return;
    }

    if (Dx) *Dx = Mouse->Buffer[0];
    if (Dy) *Dy = Mouse->Buffer[1];
    if (Buttons) *Buttons = Mouse->Buffer[2];

    Mouse->Buffer[0] = 0;
    Mouse->Buffer[1] = 0;
    Mouse->Buffer[2] = 0;
}
