#include "xhci.h"
#include "flash.h"
#include "kmalloc.h"
#include "clock.h"
#include "str.h"
#include "debug.h"
#include "pci.h"

extern AllocPool KernelPool;

void XhciRingDoorbell(XhciController* Xhci, u8 SlotId, u8 EpId) {
    u64 doorbell = Xhci->DoorbellBase + (SlotId * 4);
    *(volatile u32*)doorbell = (u32)EpId;
    __asm__ volatile("mfence" ::: "memory");
}

void XhciAdvanceEnqueue(XhciEpRing* ring, u32 count) {
    ring->Enqueue += count;
    if (ring->Enqueue >= ring->RingSize) {
        ring->Enqueue = 0;
        ring->Cycle ^= 1;
    }
}

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

    for (int i = 0; i < 32; i++) {
        xhci->EpRings[i].Ring = NULL;
        xhci->EpRings[i].RingSize = 0;
        xhci->EpRings[i].Enqueue = 0;
        xhci->EpRings[i].Cycle = 1;
        xhci->EpRings[i].RingPhys = 0;
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
    *(volatile u64*)((u64)opBase + 0x18) = (u64)xhci->CmdRing | 1ULL;

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

    u32 dboff = *(volatile u32*)((u64)MmioBase + 0x14);
    xhci->DoorbellBase = MmioBase + (dboff & ~0x3);

    *(volatile u32*)((u64)irBase + 0x00) = 0x00;
    *(volatile u32*)((u64)irBase + 0x04) = 0x00;
    *(volatile u32*)((u64)irBase + 0x08) = 1;
    *(volatile u64*)((u64)irBase + 0x10) = (u64)erst;

    *(volatile u64*)((u64)irBase + 0x18) = evRingPhys | 0x1ULL;

    *(volatile u32*)((u64)opBase + 0x00) &= ~(1 << 2);
    *(volatile u32*)((u64)opBase + 0x00) |= (1 << 0);

    *(volatile u32*)((u64)irBase+0x00)=0;
    *(volatile u32*)((u64)irBase+0x04)=0;

    for (volatile u32 i = 0; i < 1000000; i++) { __asm__ volatile ("pause"); }

    *(volatile u32*)((u64)opBase+0x04)&=~(0x0000003E);

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

    MemCopy(Event, trb, sizeof(XhciTrb));

    Xhci->EvDequeue++;
    if (Xhci->EvDequeue >= Xhci->EvRingSize) {
        Xhci->EvDequeue = 0;
        Xhci->EvCycle ^= 1;
    }

    u64 erdp = (u64)&Xhci->EvRing[Xhci->EvDequeue];
    erdp &= ~0xFULL;
    *(volatile u64*)(Xhci->RtBase + 0x38) = erdp | 1;

    MemFullFlash();
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
    __asm__ volatile("mfence" ::: "memory");
    for (volatile int i = 0; i < 100; i++) __asm__ volatile("pause");
}

u8 XhciEnableSlot(XhciController* Xhci) {
    XhciSendCommand(Xhci, 0, 0, XHCI_TRB_ENABLE_SLOT, 0);

    XhciTrb ev;
    u32 timeout = 5000000;
    while (timeout--) {
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
        __asm__ volatile("pause");
    }
    return 0;
}

u8 XhciAddressDevice(XhciController* Xhci, u8 SlotId, XhciInputContext* InputCtx) {
    XhciSendCommand(Xhci, (u64)InputCtx, 0, XHCI_TRB_ADDRESS_DEVICE, SlotId);

    XhciTrb ev;
    u32 timeout = 5000000;
    while (timeout--) {
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            if (type == XHCI_TRB_COMMAND_COMPLETE) {
                u8 code = (ev.Status >> 24) & 0xFF;
                return code;
            }
        }
        __asm__ volatile("pause");
    }
    return 0xFF;
}

static u8 XhciAllocEp0Ring(XhciController* Xhci, u8 SlotId) {
    if (SlotId >= 32) return 0;
    
    XhciEpRing* ring = &Xhci->EpRings[SlotId];
    ring->RingSize = 64;
    ring->Ring = (XhciTrb*)AlignedAlloc(&KernelPool, sizeof(XhciTrb) * ring->RingSize, 64);
    if (!ring->Ring) return 0;
    
    MemSet(ring->Ring, 0, sizeof(XhciTrb) * ring->RingSize);
    ring->Enqueue = 0;
    ring->Cycle = 1;
    ring->RingPhys = (u64)ring->Ring;
    
    return 1;
}

u8 XhciEnumerateDevice(XhciController* Xhci, u8 PortId, u8 Speed) {
    u8 slotId = XhciEnableSlot(Xhci);
    if (slotId == 0) return 0;

    if (!XhciAllocEp0Ring(Xhci, slotId)) {
        return 0;
    }

    XhciDeviceContext* devCtx = (XhciDeviceContext*)AlignedAlloc(&KernelPool, sizeof(XhciDeviceContext), 64);
    MemSet(devCtx, 0, sizeof(XhciDeviceContext));
    Xhci->Dcbaa->DevCtxPtr[slotId] = (u64)devCtx;

    XhciInputContext* inputCtx = (XhciInputContext*)AlignedAlloc(&KernelPool, sizeof(XhciInputContext), 64);
    MemSet(inputCtx, 0, sizeof(XhciInputContext));
    inputCtx->InputCtrl.AddFlags = (1 << 0) | (1 << 1);

    inputCtx->Slot.DevInfo = (PortId << 16) | (1 << 27);
    if (Speed == XHCI_PORTSPEED_LOW || Speed == XHCI_PORTSPEED_FULL) {
        inputCtx->Slot.DevInfo |= (1 << 22);
    }

    u32 maxPacketSize = 8;
    if (Speed == XHCI_PORTSPEED_SUPER) maxPacketSize = 512;
    else if (Speed == XHCI_PORTSPEED_HIGH) maxPacketSize = 64;

    XhciEpRing* ring = &Xhci->EpRings[slotId];
    inputCtx->Ep[0].EpInfo2 = (maxPacketSize << 16) | (4 << 3) | 3;
    inputCtx->Ep[0].DequeueLow = (u32)(ring->RingPhys & 0xFFFFFFFF) | 1;
    inputCtx->Ep[0].DequeueHigh = (u32)(ring->RingPhys >> 32);

    u8 code = XhciAddressDevice(Xhci, slotId, inputCtx);
    if (code != XHCI_CMPLT_SUCCESS) {
        Free(&KernelPool, devCtx);
        Free(&KernelPool, inputCtx);
        return 0;
    }

    Free(&KernelPool, inputCtx);
    XhciParseDevice(Xhci, slotId);
    return slotId;
}

u8 XhciControlTransfer(XhciController* Xhci, u8 SlotId, u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, void* Buffer) {
    XhciEpRing* ring = &Xhci->EpRings[SlotId];
    if (!ring->Ring) return 0xFF;
    
    u32 enqueue = ring->Enqueue;
    u32 cycle = ring->Cycle;
    u32 size = ring->RingSize;
    XhciTrb* trb;

    trb = &ring->Ring[enqueue];
    trb->ParameterLow = ((u32)bRequest << 8) | bmRequestType;
    trb->ParameterHigh = ((u32)wIndex << 16) | wValue;
    trb->Status = wLength;
    trb->Control = (XHCI_TRB_SETUP << 10) | (3 << 16) | cycle;
    enqueue = (enqueue + 1) % size;
    if (enqueue == 0) cycle ^= 1;

    if (wLength > 0) {
        u32 dir = (bmRequestType & 0x80) ? (1 << 16) : 0;
        u64 bufferPhys = (u64)Buffer;
        
        trb = &ring->Ring[enqueue];
        trb->ParameterLow = (u32)(bufferPhys & 0xFFFFFFFF);
        trb->ParameterHigh = (u32)(bufferPhys >> 32);
        trb->Status = wLength;
        trb->Control = (XHCI_TRB_DATA << 10) | dir | (1 << 17) | cycle;
        enqueue = (enqueue + 1) % size;
        if (enqueue == 0) cycle ^= 1;
    }

    trb = &ring->Ring[enqueue];
    trb->ParameterLow = 0;
    trb->ParameterHigh = 0;
    trb->Status = 0;
    trb->Control = (XHCI_TRB_STATUS << 10) | (1 << 16) | cycle;
    enqueue = (enqueue + 1) % size;
    if (enqueue == 0) cycle ^= 1;

    ring->Enqueue = enqueue;
    ring->Cycle = cycle;

    XhciRingDoorbell(Xhci, SlotId, 0);

    u32 timeout = 1000000;
    XhciTrb ev;
    while (timeout--) {
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            if (type == XHCI_TRB_TRANSFER_EVENT) {
                u8 slot = (ev.Control >> 24) & 0xFF;
                if (slot == SlotId) {
                    return (ev.Status >> 24) & 0xFF;
                }
            }
        }
        __asm__ volatile("pause");
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

void XhciPollEvents(XhciController* Xhci) {
    XhciTrb Event;
    
    while (XhciReadEvent(Xhci, &Event)) {
        u8 Type = (Event.Control >> 10) & 0x3F;
        u8 SlotId = (Event.Control >> 24) & 0xFF;
        u8 CompletionCode = (Event.Status >> 24) & 0xFF;

        switch (Type) {
            case XHCI_TRB_COMMAND_COMPLETE:
                break;

            case XHCI_TRB_TRANSFER_EVENT:
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
                    Port->Portsc = Portsc | XHCI_PORTSC_CSC;
                }

                if (Portsc & XHCI_PORTSC_PRC) {
                    Port->Portsc = Portsc | XHCI_PORTSC_PRC;
                }

                if (Portsc & XHCI_PORTSC_PEC) {
                    Port->Portsc = Portsc | XHCI_PORTSC_PEC;
                }

                if (Portsc & XHCI_PORTSC_WRC) {
                    Port->Portsc = Portsc | XHCI_PORTSC_WRC;
                }
                break;
            }
        }
    }
}

void XhciScanPorts(XhciController* Xhci) {
    u8 maxPorts = (u8)((Xhci->Cap->Hcsparams1 >> 24) & 0xFF);

    for (u8 i = 1; i <= maxPorts; i++) {
        XhciPortRegs* port = &Xhci->Ports[i - 1];
        u32 portsc = port->Portsc;

        if (portsc != 0 && (portsc & XHCI_PORTSC_CCS)) {
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
