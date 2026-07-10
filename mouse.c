#include "mouse.h"
#include "debug.h"
#include "kmalloc.h"
#include "str.h"
#include "keyboard.h"

extern AllocPool KernelPool;

static void XhciRingEpDoorbell(XhciController* Xhci, u8 SlotId, u8 EpId) {
    volatile u32* db = (volatile u32*)(Xhci->DoorbellBase + (SlotId * 4));
    *db = EpId;
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

    u8 dci = (Mouse->EpIn * 2) + 1; 
    u8 epIdx = dci - 1;
    inputCtx->InputCtrl.AddFlags = (1 << dci);

    inputCtx->Ep[epIdx].DequeueLow = (u32)(Mouse->TrbRingPhys & 0xFFFFFFFF) | 1;
    inputCtx->Ep[epIdx].DequeueHigh = (u32)(Mouse->TrbRingPhys >> 32);
    inputCtx->Ep[epIdx].EpInfo2 = (Mouse->MaxPacketSize << 16) | (3 << 3) | 3;

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

void MousePrepareTransfer(MouseDevice* Mouse) {
    u32 idx = Mouse->Enqueue;
    
    u64 bufferPhys = (u64)Mouse->ReportBuffer;

    Mouse->TrbRing[idx].ParameterLow = (u32)(bufferPhys & 0xFFFFFFFF);
    Mouse->TrbRing[idx].ParameterHigh = (u32)(bufferPhys >> 32);
    Mouse->TrbRing[idx].Status = Mouse->MaxPacketSize;
    Mouse->TrbRing[idx].Control = (XHCI_TRB_NORMAL << 10) | XHCI_TRB_IOC | Mouse->Ccs;

    Mouse->Enqueue++;
    if (Mouse->Enqueue >= 63) {
        XhciTrb* link = &Mouse->TrbRing[Mouse->Enqueue];
        link->ParameterLow = (u32)(Mouse->TrbRingPhys & 0xFFFFFFFF);
        link->ParameterHigh = (u32)(Mouse->TrbRingPhys >> 32);
        link->Status = 0;
        link->Control = (XHCI_TRB_LINK << 10) | Mouse->Ccs;
        
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
        if (XhciControlTransfer(Xhci, slot, 0x80, USB_REQ_GET_DESCRIPTOR,
                                (USB_DT_DEVICE << 8), 0, 18, deviceDescBuf) != XHCI_CMPLT_SUCCESS) {
            continue;
        }

        UsbDeviceDescriptor* deviceDesc = (UsbDeviceDescriptor*)deviceDescBuf;
        if (deviceDesc->bDeviceClass == 0 && deviceDesc->bNumConfigurations > 0) {
            u8 configDescBuf[256];
            if (XhciControlTransfer(Xhci, slot, 0x80, USB_REQ_GET_DESCRIPTOR,
                                    (USB_DT_CONFIG << 8), 0, 9, configDescBuf) != XHCI_CMPLT_SUCCESS) {
                continue;
            }

            u16 totalLength = *(u16*)(configDescBuf + 2);
            if (XhciControlTransfer(Xhci, slot, 0x80, USB_REQ_GET_DESCRIPTOR,
                                    (USB_DT_CONFIG << 8), 0, totalLength, configDescBuf) != XHCI_CMPLT_SUCCESS) {
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
                if (XhciControlTransfer(Xhci, slot, 0x00, USB_REQ_SET_CONFIG,
                                        deviceDesc->bNumConfigurations, 0, 0, NULL) != XHCI_CMPLT_SUCCESS) {
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
    return 0;
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
                u64 completedTrbPhys = ev.ParameterLow | ((u64)ev.ParameterHigh << 32);
                XhciTrb* completedTrb = (XhciTrb*)completedTrbPhys;
                
                u8* report = (u8*)((u64)completedTrb->ParameterLow | ((u64)completedTrb->ParameterHigh << 32));
                
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
                }
                
                MousePrepareTransfer(Mouse);
                
                u8 dci = (Mouse->EpIn * 2) + 1;
                XhciRingEpDoorbell(xhci, Mouse->SlotId, dci);
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