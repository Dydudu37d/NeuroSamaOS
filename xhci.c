#include "xhci.h"
#include "kmalloc.h"
#include "clock.h"
#include "str.h"
#include "debug.h"
#include "pci.h"

extern AllocPool KernelPool;

u64 XhciGetMmioBase(u8 Bus, u8 Slot, u8 Func) {
    return PCIGetBARAddress(Bus, Slot, Func, 0);
}

XhciController* XhciInit(u64 MmioBase) {
    u32 hccparams1 = *(volatile u32*)((u64)MmioBase + 0x10);
    u32 xecp = (hccparams1 >> 16) & 0xFFFF;
    if (xecp != 0) {
        u64 ecapBase = MmioBase + (xecp << 2);
        while (1) {
            u32 capId = *(volatile u32*)(ecapBase) & 0xFF;
            if (capId == 1) {
                u32 legacyAuth = *(volatile u32*)(ecapBase);
                if (legacyAuth & (1 << 16)) {
                    *(volatile u8*)(ecapBase + 3) |= (1 << 0);
                    u32 handshakeTimeout = 100000;
                    while ((*(volatile u32*)(ecapBase) & (1 << 16)) && handshakeTimeout--) {
                        __asm__ volatile ("pause");
                    }
                }
                break;
            }
            u32 next = (*(volatile u32*)(ecapBase) >> 8) & 0xFF;
            if (next == 0) break;
            ecapBase += (next << 2);
        }
    }

    u8 capLength = *(volatile u8*)((u64)MmioBase);
    u64 opBase = MmioBase + capLength;                                                                                                                                                                                                        
    *(volatile u32*)((u64)opBase + 0x00) &= ~(1 << 0);
    u32 timeout = 200000;
    while (!(*(volatile u32*)((u64)opBase + 0x04) & (1 << 0)) && timeout--) { __asm__ volatile ("pause"); }

    *(volatile u32*)((u64)opBase + 0x00) |= (1 << 1);
    timeout = 200000;
    while ((*(volatile u32*)((u64)opBase + 0x00) & (1 << 1)) && timeout--) { __asm__ volatile ("pause"); }

    timeout = 200000;
    while ((*(volatile u32*)((u64)opBase + 0x04) & 1) && timeout--) __asm__ volatile ("pause");

    timeout = 200000;
    while ((*(volatile u32*)((u64)opBase + 0x04) & (1 << 11)) && timeout--) { __asm__ volatile ("pause"); }

    XhciController* xhci = (XhciController*)Alloc(&KernelPool, sizeof(XhciController));
    if (!xhci) {
        DebugStr("XHCI ALLOC FAILED\n");
        return NULL;
    }

    xhci->MmioBase = MmioBase;
    xhci->Cap = (XhciCapRegs*)MmioBase;
    xhci->Op = (XhciOpRegs*)opBase;
    xhci->Ports = (XhciPortRegs*)(opBase + 0x400);

    xhci->MaxSlots = (u8)(*(volatile u32*)((u64)MmioBase + 0x04) & 0xFF);
    *(volatile u32*)((u64)opBase + 0x38) = (u32)xhci->MaxSlots;

    xhci->Dcbaa = (XhciDcbaa*)AlignedAlloc(&KernelPool, sizeof(XhciDcbaa), 64);
    MemSet(xhci->Dcbaa, 0, sizeof(XhciDcbaa));
    *(volatile u64*)((u64)opBase + 0x30) = (u64)xhci->Dcbaa;

    xhci->CmdRingSize = 64;
    xhci->CmdRing = (XhciTrb*)AlignedAlloc(&KernelPool, sizeof(XhciTrb) * xhci->CmdRingSize, 64);
    MemSet(xhci->CmdRing, 0, sizeof(XhciTrb) * xhci->CmdRingSize);
    xhci->CmdEnqueue = 0;
    xhci->CmdCycle = 1;
    *(volatile u64*)((u64)opBase + 0x18) = (u64)xhci->CmdRing | (u64)xhci->CmdCycle;

    xhci->EvRingSize = 64;
    xhci->EvRing = (XhciTrb*)AlignedAlloc(&KernelPool, sizeof(XhciTrb) * xhci->EvRingSize, 64);
    MemSet(xhci->EvRing, 0, sizeof(XhciTrb) * xhci->EvRingSize);
    xhci->EvDequeue = 0;
    xhci->EvCycle = 1;

    u32* erst = (u32*)AlignedAlloc(&KernelPool, 64, 64);
    MemSet(erst, 0, 64);
    u64 evRingPhys = (u64)xhci->EvRing;
    erst[0] = (u32)(evRingPhys & 0xFFFFFFFF);
    erst[1] = (u32)(evRingPhys >> 32);
    erst[2] = (u32)(xhci->EvRingSize & 0xFFFF);
    xhci->Erst = (u64*)erst;

    xhci->Rtsoff = *(volatile u32*)((u64)MmioBase + 0x18);
    u64 rtBase = MmioBase + xhci->Rtsoff;
    xhci->RtBase = rtBase;

    u64 irBase = rtBase + 0x20;

    xhci->DoorbellBase = opBase + 0x40;

    *(volatile u32*)((u64)irBase + 0x00) = 0x00;
    *(volatile u32*)((u64)irBase + 0x08) = 1;
    *(volatile u64*)((u64)irBase + 0x10) = (u64)erst;
    
    DebugStr("ERSTBA written, readback: ");
    DebugU64(*(volatile u64*)((u64)irBase + 0x10));
    DebugStr("\nExpected: ");
    DebugU64((u64)erst);
    DebugStr("\n");
    
    *(volatile u64*)((u64)irBase + 0x18) = (u64)evRingPhys | 0x1ULL;

    *(volatile u32*)((u64)opBase + 0x00) |= (1 << 0);

    for (volatile u32 i = 0; i < 1000000; i++) { __asm__ volatile ("pause"); }

    u32 usbsts = *(volatile u32*)((u64)opBase + 0x04);
    DebugStr("USBSTS after start: ");
    DebugU32(usbsts);
    DebugStr("\n");

    XhciTrb* ev = xhci->EvRing;
    DebugStr("EvRing[0] Control: ");
    DebugU32(ev[0].Control);
    DebugStr("\n");
    DebugStr("EvRing[1] Control: ");
    DebugU32(ev[1].Control);
    DebugStr("\n");
    DebugStr("EvRing[2] Control: ");
    DebugU32(ev[2].Control);
    DebugStr("\n");
    DebugStr("EvRing[3] Control: ");
    DebugU32(ev[3].Control);
    DebugStr("\n");

    DebugStr("ERDP reg: ");
    DebugU64(*(volatile u64*)((u64)irBase + 0x18));
    DebugStr("\n");

    DebugStr("ERSTBA reg: ");
    DebugU64(*(volatile u64*)((u64)irBase + 0x10));
    DebugStr("\n");

    DebugStr("EvRing VA: ");
    DebugU64((u64)xhci->EvRing);
    DebugStr("\n");
    DebugStr("EvRing PA (from ERST): ");
    DebugU64(((u64)erst[1] << 32) | erst[0]);
    DebugStr("\n");

    DebugStr("MMIO base: ");
    DebugU64(MmioBase);
    DebugStr("\n");
    DebugStr("opBase: ");
    DebugU64(opBase);
    DebugStr("\n");
    u32 usbcmd = *(volatile u32*)((u64)opBase + 0x00);
    DebugStr("USBCMD: ");
    DebugU32(usbcmd);
    DebugStr("\n");

    DebugStr("RTSOFF: ");
    DebugU32(xhci->Rtsoff);
    DebugStr("\n");
    DebugStr("rtBase: ");
    DebugU64(rtBase);
    DebugStr("\n");

    timeout = 500000;
    while (timeout--) {
        u32 sts = *(volatile u32*)((u64)opBase + 0x04);
        if (!(sts & 1)) {
            DebugStr("XHCI SUCCESS: Running!\n");
            return xhci;
        }
        if (sts & 0x0000000C) {
            DebugStr("XHCI FATAL ERROR: 0x");
            DebugU32(sts);
            return NULL;
        }
        __asm__ volatile ("pause");
    }
    DebugStr("TimeOut\n");
    return NULL;
}

u8 XhciReadEvent(XhciController* Xhci, XhciTrb* Event) {
    XhciTrb* trb = &Xhci->EvRing[Xhci->EvDequeue];
    u32 control = trb->Control;

    if ((control & 1) != Xhci->EvCycle) {
        return 0;
    }

    Event->ParameterLow = trb->ParameterLow;
    Event->ParameterHigh = trb->ParameterHigh;
    Event->Status = trb->Status;
    Event->Control = trb->Control;

    trb->Control = 0;

    Xhci->EvDequeue++;
    if (Xhci->EvDequeue >= Xhci->EvRingSize) {
        Xhci->EvDequeue = 0;
        Xhci->EvCycle ^= 1;
    }

    u64 erdp = (u64)&Xhci->EvRing[Xhci->EvDequeue];
    *(volatile u64*)(Xhci->RtBase + 0x20 + 0x18) = erdp | 0x1ULL;

    return 1;
}

void XhciSendCommand(XhciController* Xhci, u64 Param, u32 Status, u8 Type, u8 SlotId) {
    u32 index = Xhci->CmdEnqueue;
    XhciTrb* trb = &Xhci->CmdRing[index];

    trb->ParameterLow = (u32)(Param & 0xFFFFFFFF);
    trb->ParameterHigh = (u32)(Param >> 32);
    trb->Status = Status;
    trb->Control = ((u32)Type << 10) | ((u32)SlotId << 24) | Xhci->CmdCycle;

    Xhci->CmdEnqueue++;
    if (Xhci->CmdEnqueue >= Xhci->CmdRingSize - 1) {
        XhciTrb* link = &Xhci->CmdRing[Xhci->CmdEnqueue];
        link->ParameterLow = (u32)((u64)Xhci->CmdRing & 0xFFFFFFFF);
        link->ParameterHigh = (u32)((u64)Xhci->CmdRing >> 32);
        link->Status = 0;
        link->Control = ((u32)XHCI_TRB_LINK << 10) | (1 << 1) | Xhci->CmdCycle;

        Xhci->CmdEnqueue = 0;
        Xhci->CmdCycle ^= 1;
    }

    *(volatile u32*)(Xhci->DoorbellBase) = 0;
}

u8 XhciEnableSlot(XhciController* Xhci) {
    XhciSendCommand(Xhci, 0, 0, XHCI_TRB_ENABLE_SLOT, 0);

    XhciTrb ev;
    while (1) {
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            if (type == XHCI_TRB_COMMAND_COMPLETE) {
                u8 slotId = (ev.Control >> 24) & 0xFF;
                u8 code = (ev.Status >> 24) & 0xFF;
                if (code == XHCI_CMPLT_SUCCESS) {
                    return slotId;
                }
                return 0;
            }
        }
    }
}

u8 XhciAddressDevice(XhciController* Xhci, u8 SlotId, XhciInputContext* InputCtx) {
    XhciSendCommand(Xhci, (u64)InputCtx, 0, XHCI_TRB_ADDRESS_DEVICE, SlotId);

    XhciTrb ev;
    while (1) {
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            if (type == XHCI_TRB_COMMAND_COMPLETE) {
                u8 code = (ev.Status >> 24) & 0xFF;
                return code;
            }
        }
    }
}

void XhciScanPorts(XhciController* Xhci) {
    u8 maxPorts = (u8)((Xhci->Cap->Hcsparams1 >> 24) & 0xFF);

    for (u8 i = 1; i <= maxPorts; i++) {
        XhciPortRegs* port = &Xhci->Ports[i - 1];
        u32 portsc = port->Portsc;

        if (portsc != 0) {
            portsc &= ~0x0E01CE0E;
            portsc |= XHCI_PORTSC_PR;
            port->Portsc = portsc;

            SystemBusySleepMs(20);

            portsc = port->Portsc;
            if (portsc & XHCI_PORTSC_PED) {
                u8 speed = (portsc >> 10) & 0x0F;
                XhciEnumerateDevice(Xhci, i, speed);
            }
        }
    }
}

u8 XhciEnumerateDevice(XhciController* Xhci, u8 PortId, u8 Speed) {
    u8 slotId = XhciEnableSlot(Xhci);
    if (slotId == 0) {
        return 0;
    }

    XhciDeviceContext* devCtx = (XhciDeviceContext*)AlignedAlloc(&KernelPool, sizeof(XhciDeviceContext), 64);
    Xhci->Dcbaa->DevCtxPtr[slotId] = (u64)devCtx;

    XhciInputContext* inputCtx = (XhciInputContext*)AlignedAlloc(&KernelPool, sizeof(XhciInputContext), 64);
    inputCtx->InputCtrl.AddFlags = (1 << 0) | (1 << 1);

    inputCtx->Slot.DevInfo = (PortId << 16) | (1 << 27);
    if (Speed == XHCI_PORTSPEED_LOW || Speed == XHCI_PORTSPEED_FULL) {
        inputCtx->Slot.DevInfo |= (1 << 22);
    }

    u32 maxPacketSize = 8;
    if (Speed == XHCI_PORTSPEED_SUPER) maxPacketSize = 512;
    else if (Speed == XHCI_PORTSPEED_HIGH) maxPacketSize = 64;

    inputCtx->Ep[0].EpInfo2 = (maxPacketSize << 16) | (4 << 3) | 3;

    u8 code = XhciAddressDevice(Xhci, slotId, inputCtx);
    if (code != XHCI_CMPLT_SUCCESS) {
        return 0;
    }
    
    XhciParseDevice(Xhci, slotId);

    return slotId;
}

void XhciPollEvents(XhciController* Xhci) {
    while (1) {
        XhciTrb Event;
        if (!XhciReadEvent(Xhci, &Event)) {
            break;
        }

        u8 Type = (Event.Control >> 10) & 0x3F;
        u8 SlotId = (Event.Control >> 24) & 0xFF;
        u8 CompletionCode = (Event.Status >> 24) & 0xFF;

        switch (Type) {
            case XHCI_TRB_COMMAND_COMPLETE:
                break;

            case XHCI_TRB_TRANSFER_EVENT:
                if (CompletionCode == XHCI_CMPLT_SUCCESS) {
                }
                break;

            case XHCI_TRB_PORT_STATUS_CHANGE: {
                u32 PortId = (Event.Status >> 16) & 0xFF;
                XhciPortRegs* Port = &Xhci->Ports[PortId - 1];
                u32 Portsc = Port->Portsc;

                if (Portsc & XHCI_PORTSC_CSC) {
                    if (Portsc & XHCI_PORTSC_CCS) {
                        u8 Speed = (Portsc >> 10) & 0x0F;
                        XhciEnumerateDevice(Xhci, PortId, Speed);
                    }
                    Portsc |= XHCI_PORTSC_CSC;
                    Port->Portsc = Portsc;
                }

                if (Portsc & XHCI_PORTSC_PRC) {
                    Portsc |= XHCI_PORTSC_PRC;
                    Port->Portsc = Portsc;
                }

                if (Portsc & XHCI_PORTSC_PEC) {
                    Portsc |= XHCI_PORTSC_PEC;
                    Port->Portsc = Portsc;
                }

                if (Portsc & XHCI_PORTSC_WRC) {
                    Portsc |= XHCI_PORTSC_WRC;
                    Port->Portsc = Portsc;
                }
                break;
            }
        }
    }
}

u8 XhciControlTransfer(XhciController* Xhci, u8 SlotId, u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, void* Buffer) {
    u32 setupIndex = Xhci->CmdEnqueue;
    XhciTrb* setupTrb = &Xhci->CmdRing[setupIndex];
    setupTrb->ParameterLow = ((u32)bRequest << 8) | bmRequestType;
    setupTrb->ParameterHigh = ((u32)wIndex << 16) | wValue;
    setupTrb->Status = wLength;
    setupTrb->Control = (XHCI_TRB_SETUP << 10) | (3 << 16) | Xhci->CmdCycle;
    
    Xhci->CmdEnqueue++;
    if (Xhci->CmdEnqueue >= Xhci->CmdRingSize - 1) {
        XhciTrb* link = &Xhci->CmdRing[Xhci->CmdEnqueue];
        link->ParameterLow = (u32)((u64)Xhci->CmdRing & 0xFFFFFFFF);
        link->ParameterHigh = (u32)((u64)Xhci->CmdRing >> 32);
        link->Status = 0;
        link->Control = (XHCI_TRB_LINK << 10) | (1 << 1) | Xhci->CmdCycle;
        Xhci->CmdEnqueue = 0;
        Xhci->CmdCycle ^= 1;
    }
    
    u32 dataCycle = Xhci->CmdCycle;
    u32 dataEnqueue = Xhci->CmdEnqueue;
    
    if (wLength > 0) {
        u32 dir = (bmRequestType & 0x80) ? (1 << 16) : 0;
        u64 bufferPhys = (u64)Buffer;
        
        XhciTrb* dataTrb = &Xhci->CmdRing[dataEnqueue];
        dataTrb->ParameterLow = (u32)(bufferPhys & 0xFFFFFFFF);
        dataTrb->ParameterHigh = (u32)(bufferPhys >> 32);
        dataTrb->Status = wLength;
        dataTrb->Control = (XHCI_TRB_DATA << 10) | dir | (1 << 17) | dataCycle;
        
        Xhci->CmdEnqueue++;
        if (Xhci->CmdEnqueue >= Xhci->CmdRingSize - 1) {
            XhciTrb* link = &Xhci->CmdRing[Xhci->CmdEnqueue];
            link->ParameterLow = (u32)((u64)Xhci->CmdRing & 0xFFFFFFFF);
            link->ParameterHigh = (u32)((u64)Xhci->CmdRing >> 32);
            link->Status = 0;
            link->Control = (XHCI_TRB_LINK << 10) | (1 << 1) | dataCycle;
            Xhci->CmdEnqueue = 0;
            Xhci->CmdCycle ^= 1;
        }
    }
    
    u32 statusCycle = Xhci->CmdCycle;
    XhciTrb* statusTrb = &Xhci->CmdRing[Xhci->CmdEnqueue];
    statusTrb->ParameterLow = 0;
    statusTrb->ParameterHigh = 0;
    statusTrb->Status = 0;
    statusTrb->Control = (XHCI_TRB_STATUS << 10) | (1 << 16) | statusCycle;
    
    Xhci->CmdEnqueue++;
    if (Xhci->CmdEnqueue >= Xhci->CmdRingSize - 1) {
        XhciTrb* link = &Xhci->CmdRing[Xhci->CmdEnqueue];
        link->ParameterLow = (u32)((u64)Xhci->CmdRing & 0xFFFFFFFF);
        link->ParameterHigh = (u32)((u64)Xhci->CmdRing >> 32);
        link->Status = 0;
        link->Control = (XHCI_TRB_LINK << 10) | (1 << 1) | statusCycle;
        Xhci->CmdEnqueue = 0;
        Xhci->CmdCycle ^= 1;
    }
    
    *(volatile u32*)(Xhci->DoorbellBase) = 0;
    
    u32 timeout = 1000000;
    XhciTrb ev;
    while (timeout--) {
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            if (type == XHCI_TRB_COMMAND_COMPLETE || type == XHCI_TRB_TRANSFER_EVENT) {
                return (ev.Status >> 24) & 0xFF;
            }
        }
        __asm__ volatile ("pause");
    }
    
    return 0xFF;
}

void XhciGetDeviceDescriptor(XhciController* Xhci, u8 SlotId, UsbDeviceDescriptor* Desc) {
    XhciControlTransfer(Xhci, SlotId, 0x80, USB_REQ_GET_DESCRIPTOR, 
                        (USB_DT_DEVICE << 8) | 0, 0, sizeof(UsbDeviceDescriptor), Desc);
}

void XhciParseDevice(XhciController* Xhci, u8 SlotId) {
    UsbDeviceDescriptor devDesc;
    XhciGetDeviceDescriptor(Xhci, SlotId, &devDesc);
    
    u8 configHeader[9];
    XhciControlTransfer(Xhci, SlotId, 0x80, USB_REQ_GET_DESCRIPTOR,
                        (USB_DT_CONFIG << 8) | 0, 0, 9, configHeader);
    
    u16 totalLen = *(u16*)(configHeader + 2);
    u8* configDesc = (u8*)Alloc(&KernelPool, totalLen);
    if (!configDesc) return;
    
    XhciControlTransfer(Xhci, SlotId, 0x80, USB_REQ_GET_DESCRIPTOR,
                        (USB_DT_CONFIG << 8) | 0, 0, totalLen, configDesc);
    
    u8* ptr = configDesc;
    u8* end = ptr + totalLen;
    
    while (ptr < end) {
        u8 descLen = ptr[0];
        u8 descType = ptr[1];
        
        if (descType == USB_DT_INTERFACE) {
            UsbInterfaceDescriptor* iface = (UsbInterfaceDescriptor*)ptr;
            
            if (iface->bInterfaceClass == USB_CLASS_HID && 
                iface->bInterfaceSubClass == USB_SUBCLASS_BOOT) {
                
                if (iface->bInterfaceProtocol == USB_PROTOCOL_KEYBOARD) {
                    DebugStr("USB Keyboard found on slot ");
                    DebugU32(SlotId);
                    DebugStr("\n");
                }
                else if (iface->bInterfaceProtocol == USB_PROTOCOL_MOUSE) {
                    DebugStr("USB Mouse found on slot ");
                    DebugU32(SlotId);
                    DebugStr("\n");
                }
            }
        }
        
        ptr += descLen;
    }
    
    Free(&KernelPool, configDesc);
}
