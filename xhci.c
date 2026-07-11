#include "xhci.h"
#include "flash.h"
#include "kmalloc.h"
#include "clock.h"
#include "str.h"
#include "debug.h"
#include "pci.h"
#include "keyboard.h"
#include "mouse.h"

#define TRB_TR_NOOP 8

#define ADD_TRB(low, high, status, ctrl) do { \
    u32 next_index = (enqueue + 1) % size; \
    if (next_index == size - 1) { \
        trb = &ring->Ring[enqueue]; \
        trb->ParameterLow = 0; \
        trb->ParameterHigh = 0; \
        trb->Status = 0; \
        trb->Control = (TRB_TR_NOOP << 10) | cycle; \
        FlushCache(trb, sizeof(XhciTrb)); \
        enqueue = next_index; \
    } \
    if (enqueue == size - 1) { \
        XhciTrb* link = &ring->Ring[enqueue]; \
        link->ParameterLow = (u32)((u64)ring->Ring & 0xFFFFFFFF); \
        link->ParameterHigh = (u32)((u64)ring->Ring >> 32); \
        link->Status = 0; \
        link->Control = (6 << 10) | (1 << 1) | cycle; \
        FlushCache(link, sizeof(XhciTrb)); \
        enqueue = 0; \
        cycle ^= 1; \
    } \
    trb = &ring->Ring[enqueue]; \
    trb->ParameterLow = low; \
    trb->ParameterHigh = high; \
    trb->Status = status; \
    trb->Control = ctrl | cycle; \
    FlushCache(trb, sizeof(XhciTrb)); \
    enqueue++; \
} while(0)

MouseDevice* ActiveMouse = NULL;
KeyboardDevice* ActiveKbd = NULL;
u64 XHCI_MAX_PORTS=15;

extern AllocPool KernelPool;

void FlushCache(void* ptr, u64 size) {
    u64 start = (u64)ptr & ~0x3FULL;
    u64 end = ((u64)ptr + size + 63) & ~0x3FULL;
    for (u64 i = start; i < end; i += 64) {
        __asm__ volatile("clflush (%0)" : : "r"(i) : "memory");
    }
    __asm__ volatile("mfence" ::: "memory");
}

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
    DebugStr("XHCI Init Start.\n");
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
    for (int i = 0; i < 256; i++) {
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
    xhci->Dcbaa->DevCtxPtr[0] = 0;
    FlushCache(xhci->Dcbaa, sizeof(XhciDcbaa));
    *(volatile u64*)((u64)opBase + 0x30) = (u64)xhci->Dcbaa;

    xhci->CmdRingSize = 64;
    xhci->CmdRing = (XhciTrb*)AlignedAlloc(&KernelPool, sizeof(XhciTrb) * xhci->CmdRingSize, 4096);
    MemSet(xhci->CmdRing, 0, sizeof(XhciTrb) * xhci->CmdRingSize);
    FlushCache(xhci->CmdRing, sizeof(XhciTrb) * xhci->CmdRingSize);
    DebugStr("CmdRing Phys: "); DebugU64((u64)xhci->CmdRing); DebugStr("\n");
    xhci->CmdEnqueue = 0;
    xhci->CmdCycle = 1;

    u64 crcr_val = (u64)xhci->CmdRing | (1ULL << 5);
    *(volatile u64*)((u64)opBase + 0x18) = crcr_val;
    DebugStr("CRCR set to "); DebugU64(crcr_val); DebugStr("\n");

    xhci->EvRingSize = 64;
    xhci->EvRing = (XhciTrb*)AlignedAlloc(&KernelPool, sizeof(XhciTrb) * xhci->EvRingSize, 4096);
    MemSet(xhci->EvRing, 0, sizeof(XhciTrb) * xhci->EvRingSize);

    XhciTrb* evLink = &xhci->EvRing[xhci->EvRingSize - 1];
    evLink->ParameterLow = (u32)((u64)xhci->EvRing & 0xFFFFFFFF);
    evLink->ParameterHigh = (u32)((u64)xhci->EvRing >> 32);
    evLink->Status = 0;
    evLink->Control = (6 << 10) | (1 << 1) | 1;

    FlushCache(xhci->EvRing, sizeof(XhciTrb) * xhci->EvRingSize);
    DebugStr("EvRing Phys: "); DebugU64((u64)xhci->EvRing); DebugStr("\n");
    xhci->EvDequeue = 0;
    xhci->EvCycle = 1;

    u32* erst = (u32*)AlignedAlloc(&KernelPool, 64, 64);
    MemSet(erst, 0, 64);
    u64 evRingPhys = (u64)xhci->EvRing;
    erst[0] = (u32)(evRingPhys & 0xFFFFFFFF);
    erst[1] = (u32)(evRingPhys >> 32);
    erst[2] = (u32)xhci->EvRingSize;
    xhci->Erst = (u64*)erst;
    FlushCache(erst, 64);

    DebugStr("EV Ring Phys: "); DebugU64(evRingPhys); DebugStr(" ERST: "); DebugU64((u64)erst); DebugStr("\n");

    xhci->Rtsoff = *(volatile u32*)((u64)MmioBase + 0x18);
    u64 rtBase = MmioBase + xhci->Rtsoff;
    xhci->RtBase = rtBase;
    u64 irBase = rtBase + 0x20;
    u32 dboff = *(volatile u32*)((u64)MmioBase + 0x14);
    xhci->DoorbellBase = MmioBase + (dboff & ~0x3);

    *(volatile u32*)((u64)irBase + 0x00) = 0;
    *(volatile u32*)((u64)irBase + 0x04) = 0;
    *(volatile u32*)((u64)irBase + 0x08) = 1;
    *(volatile u64*)((u64)irBase + 0x10) = (u64)erst;
    *(volatile u64*)((u64)irBase + 0x18) = evRingPhys | (1ULL << 3);

    DebugStr("IR Set: ERST="); DebugU64((u64)erst); DebugStr(" ERDP="); DebugU64(evRingPhys); DebugStr("\n");

    *(volatile u32*)((u64)opBase + 0x00) &= ~(1 << 2);
    *(volatile u32*)((u64)opBase + 0x00) |= (1 << 0) | (1 << 2);

    *(volatile u64*)((u64)opBase + 0x18) = (u64)xhci->CmdRing | (1ULL << 5);
    *(volatile u64*)((u64)irBase + 0x18) = evRingPhys | (1ULL << 3);

    *(volatile u32*)((u64)irBase + 0x00) = 1;
    *(volatile u32*)((u64)irBase + 0x04) = 0;

    u32 hcsparams1 = *(volatile u32*)((u64)MmioBase + 0x04);
    XHCI_MAX_PORTS = (u8)((hcsparams1 >> 24) & 0xFF);

    *(volatile u32*)((u64)opBase + 0x24) = (1 << 0) | (1 << 1);
    for (u8 p = 0; p < XHCI_MAX_PORTS; p++) {
        XhciPortRegs* port = &xhci->Ports[p];
        u32 portsc = port->Portsc;
        if (!(portsc & (1 << 3))) {
            port->Portsc = portsc | (1 << 3);
        }
    }
    for (volatile u32 i = 0; i < 2000000; i++) { __asm__ volatile ("pause"); }
    for (u8 p = 0; p < XHCI_MAX_PORTS; p++) {
        XhciPortRegs* port = &xhci->Ports[p];
        u32 portsc = port->Portsc;
        port->Portsc = portsc;
    }
    DebugStr("Ports powered on\n");

    for (volatile u32 i = 0; i < 2000000; i++) { __asm__ volatile ("pause"); }

    *(volatile u32*)((u64)opBase+0x04)&=~(0x0000003E);

    *(volatile u32*)((u64)opBase + 0x04) = 0x0C;
    DebugStr("USBSTS cleared.\n");

    DebugStr("DoorbellBase: "); DebugU64(xhci->DoorbellBase); DebugStr("\n");
    DebugStr("XHCI Init End.\n");
    return xhci;
}

u8 XhciReadEvent(XhciController* Xhci, XhciTrb* Event) {
    if (!Xhci || !Event) return 0;
    
    XhciTrb* trb = &Xhci->EvRing[Xhci->EvDequeue];
    FlushCache(trb, sizeof(XhciTrb));
    u32 control = trb->Control;
    
    DebugStr("ReadEvent: idx="); DebugU32(Xhci->EvDequeue);
    DebugStr(" control=0x"); DebugU32(control);
    DebugStr(" cycle="); DebugU32(Xhci->EvCycle);
    DebugStr(" match="); DebugU32((control & 1) == Xhci->EvCycle);
    DebugStr("\n");
    
    if (control == 0) {
        DebugStr("TRB is empty, waiting for controller...\n");
        return 0;
    }
    
    if ((control & 1) != Xhci->EvCycle) {
        return 0;
    }
    
    u8 type = (control >> 10) & 0x3F;
    
    if (type == XHCI_TRB_LINK) {
        DebugStr("Link TRB, skipping\n");
        Xhci->EvDequeue++;
        if (Xhci->EvDequeue >= Xhci->EvRingSize) {
            Xhci->EvDequeue = 0;
            Xhci->EvCycle ^= 1;
        }
        return XhciReadEvent(Xhci, Event);
    }

    MemCopy(Event, trb, sizeof(XhciTrb));

    Xhci->EvDequeue++;
    if (Xhci->EvDequeue >= Xhci->EvRingSize) {
        Xhci->EvDequeue = 0;
        Xhci->EvCycle ^= 1;
    }
    
    u64 erdp_addr = (u64)&Xhci->EvRing[Xhci->EvDequeue] & ~0xFULL;
    erdp_addr |= (1ULL << 3);
    *(volatile u64*)((u64)Xhci->RtBase + 0x38) = erdp_addr;
    __asm__ volatile("mfence" ::: "memory");
    
    return 1;
}

void XhciSendCommand(XhciController* Xhci, u64 Param, u32 Status, u8 Type, u8 SlotId) {
    u64 crcr = *(volatile u64*)((u64)Xhci->Op + 0x18);
    
    if (!(crcr & (1ULL << 5))) {
        Xhci->CmdEnqueue = 0;
        Xhci->CmdCycle = 1;
        u64 new_crcr = (u64)Xhci->CmdRing | (1ULL << 5);
        *(volatile u64*)((u64)Xhci->Op + 0x18) = new_crcr;
        DebugStr("CRCR restarted to "); DebugU64(new_crcr); DebugStr("\n");
        
        u32 timeout = 100000;
        while (!(*(volatile u64*)((u64)Xhci->Op + 0x18) & (1ULL << 5)) && timeout--) {
            __asm__ volatile("pause");
        }
        if (timeout == 0) {
            DebugStr("CRCR restart timeout!\n");
            return;
        }
    }

    u32 index = Xhci->CmdEnqueue;
    XhciTrb* trb = &Xhci->CmdRing[index];
    trb->ParameterLow = (u32)(Param & 0xFFFFFFFF);
    trb->ParameterHigh = (u32)(Param >> 32);
    trb->Status = Status;
    trb->Control = ((u32)Type << 10) | ((u32)SlotId << 24) | XHCI_TRB_IOC | Xhci->CmdCycle;
    FlushCache(trb, sizeof(XhciTrb));

    Xhci->CmdEnqueue++;
    if (Xhci->CmdEnqueue >= Xhci->CmdRingSize - 1) {
        XhciTrb* link = &Xhci->CmdRing[Xhci->CmdEnqueue];
        link->ParameterLow = (u32)((u64)Xhci->CmdRing & 0xFFFFFFFF);
        link->ParameterHigh = (u32)((u64)Xhci->CmdRing >> 32);
        link->Status = 0;
        link->Control = ((u32)XHCI_TRB_LINK << 10) | (1 << 1) | XHCI_TRB_IOC | Xhci->CmdCycle;
        FlushCache(link, sizeof(XhciTrb));
        Xhci->CmdEnqueue = 0;
        Xhci->CmdCycle ^= 1;
    }

    __asm__ volatile("mfence" ::: "memory");
    *(volatile u32*)(Xhci->DoorbellBase) = 0;
    __asm__ volatile("mfence" ::: "memory");
}

u8 XhciEnableSlot(XhciController* Xhci) {
    DebugStr("Sending Enable Slot Command\n");
    XhciSendCommand(Xhci, 0, 0, XHCI_TRB_ENABLE_SLOT, 0);
    u64 crcr = *(volatile u64*)((u64)Xhci->Op + 0x18);
    DebugStr("CRCR after doorbell: "); DebugU64(crcr); DebugStr("\n");

    u32 timeout = 5000000;
    XhciTrb ev;
    while (timeout--) {
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            DebugStr("Got Event Type: "); DebugU8(type); DebugStr("\n");
            if (type == XHCI_TRB_COMMAND_COMPLETE) {
                u8 slotId = (ev.Control >> 24) & 0xFF;
                u8 code = (ev.Status >> 24) & 0xFF;
                DebugStr("Enable Slot Complete! Slot="); DebugU8(slotId); DebugStr(" Code=0x"); DebugU8(code); DebugStr("\n");
                if (code == XHCI_CMPLT_SUCCESS) return slotId;
                return 0;
            }
        }
        __asm__ volatile("pause");
    }
    DebugStr("Enable Slot Timeout\n");
    return 0;
}

u8 XhciAddressDevice(XhciController* Xhci, u8 SlotId, XhciInputContext* InputCtx, u8 bsr) {
    DebugStr("XhciAddressDevice: SlotId="); DebugU8(SlotId);
    DebugStr(" bsr="); DebugU8(bsr); DebugStr("\n");

    FlushCache(InputCtx, 2048);
    __asm__ volatile("mfence" ::: "memory");

    u32 status = bsr ? (1 << 9) : 0;
    XhciSendCommand(Xhci, (u64)InputCtx, status, XHCI_TRB_ADDRESS_DEVICE, SlotId);

    XhciTrb ev;
    u32 timeout = 5000000;
    while (timeout--) {
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            if (type == XHCI_TRB_COMMAND_COMPLETE) {
                u8 code = (ev.Status >> 24) & 0xFF;
                DebugStr("Address Device Complete! Code=0x"); DebugU8(code); DebugStr("\n");
                return code;
            }
        }
        __asm__ volatile("pause");
    }
    DebugStr("Address Device Timeout!\n");
    return 0xFF;
}

static u8 XhciAllocEp0Ring(XhciController* Xhci, u8 SlotId) {
    if (SlotId >= 256) return 0;

    XhciEpRing* ring = &Xhci->EpRings[SlotId];
    ring->RingSize = 64;
    ring->Ring = (XhciTrb*)AlignedAlloc(&KernelPool, sizeof(XhciTrb) * ring->RingSize, 64);
    if (!ring->Ring) return 0;

    MemSet(ring->Ring, 0, sizeof(XhciTrb) * ring->RingSize);

    XhciTrb* link = &ring->Ring[ring->RingSize - 1];
    link->ParameterLow = (u32)((u64)ring->Ring & 0xFFFFFFFF);
    link->ParameterHigh = (u32)((u64)ring->Ring >> 32);
    link->Status = 0;
    link->Control = (6 << 10) | (1 << 1) | 1;

    FlushCache(ring->Ring, sizeof(XhciTrb) * ring->RingSize);
    ring->Enqueue = 0;
    ring->Cycle = 1;
    ring->RingPhys = (u64)ring->Ring;

    DebugStr("AllocEp0Ring: SlotId="); DebugU8(SlotId);
    DebugStr(" RingPhys=0x"); DebugU64(ring->RingPhys);
    DebugStr("\n");

    return 1;
}

u8 XhciEnumerateDevice(XhciController* Xhci, u8 PortId, u8 Speed) {
    DebugStr("XhciEnumerateDevice: PortId="); DebugU8(PortId);
    DebugStr(" Speed="); DebugU8(Speed); DebugStr("\n");

    u8 slotId = XhciEnableSlot(Xhci);
    if (slotId == 0) return 0;

    if (!XhciAllocEp0Ring(Xhci, slotId)) return 0;

    void* devCtx = AlignedAlloc(&KernelPool, 2048, 64);
    MemSet(devCtx, 0, 2048);
    XhciDeviceContext* dev = (XhciDeviceContext*)devCtx;

    dev->Slot.DevInfo = ((u32)Speed << 20) | (1u << 27);
    dev->Slot.DevInfo2 = (u32)PortId;
    dev->Slot.DevState = 0;

    Xhci->Dcbaa->DevCtxPtr[slotId] = (u64)devCtx;
    FlushCache(devCtx, 2048);
    FlushCache(&Xhci->Dcbaa->DevCtxPtr[slotId], 8);

    void* inputCtx = AlignedAlloc(&KernelPool, 2048, 64);
    MemSet(inputCtx, 0, 2048);
    XhciInputContext* input = (XhciInputContext*)inputCtx;

    input->InputCtrl.DropFlags = 0;
    input->InputCtrl.AddFlags = (1u << 0) | (1u << 1);

    input->Slot.DevInfo = ((u32)Speed << 20) | (1u << 27);
    input->Slot.DevInfo2 = (PortId << 16);
    input->Slot.TtInfo = 0;
    input->Slot.DevState = 0;

    XhciEpRing* ring = &Xhci->EpRings[slotId];

    u32 maxPacketSize = (Speed == 4) ? 512 : (Speed == 3 ? 64 : 8);
    u32 epType = 3;
    u32 cErr = 3;

    input->Ep[0].EpInfo  = 0;
    input->Ep[0].EpInfo2 = (maxPacketSize << 16) | (epType << 3) | (cErr << 1);
    input->Ep[0].DequeueLow  = ((u32)ring->RingPhys & 0xFFFFFFFF) | 1;
    input->Ep[0].DequeueHigh = (u32)(ring->RingPhys >> 32);

    DebugStr("=== Input Context Dump ===\n");
    DebugStr("AddFlags = 0x"); DebugU32(input->InputCtrl.AddFlags); DebugStr("\n");
    DebugStr("Slot.DevInfo = 0x"); DebugU32(input->Slot.DevInfo); DebugStr("\n");
    DebugStr("EP0 EpInfo = 0x"); DebugU32(input->Ep[0].EpInfo); DebugStr("\n");
    DebugStr("EP0 EpInfo2 = 0x"); DebugU32(input->Ep[0].EpInfo2); DebugStr("\n");
    DebugStr("EP0 Dequeue = 0x"); DebugU64(ring->RingPhys | 1); DebugStr("\n");
    DebugStr("==========================\n");

    FlushCache(inputCtx, 2048);
    __asm__ volatile("mfence" ::: "memory");

    u8 code = XhciAddressDevice(Xhci, slotId, inputCtx, 0);
    if (code != XHCI_CMPLT_SUCCESS) {
        DebugStr("Address Device Failed! Code=0x"); DebugU8(code); DebugStr("\n");
        Free(&KernelPool, devCtx);
        Free(&KernelPool, inputCtx);
        return 0;
    }

    DebugStr("Address Device SUCCESS! Slot="); DebugU8(slotId); DebugStr("\n");

    Free(&KernelPool, inputCtx);
    XhciParseDevice(Xhci, slotId);
    return slotId;
}

u8 XhciControlTransfer(XhciController* Xhci, u8 SlotId, u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, void* Buffer) {
    DebugStr("XhciControlTransfer: SlotId="); DebugU8(SlotId);
    DebugStr(" bmRequestType=0x"); DebugU8(bmRequestType);
    DebugStr(" bRequest=0x"); DebugU8(bRequest);
    DebugStr(" wValue=0x"); DebugU16(wValue);
    DebugStr(" wLength="); DebugU16(wLength);
    DebugStr("\n");
    
    XhciEpRing* ring = &Xhci->EpRings[SlotId];
    if (!ring->Ring) {
        DebugStr("ring->Ring is NULL!\n");
        return 0xFF;
    }

    *(volatile u32*)((u64)Xhci->Op + 0x04) = 0x0C;

    u32 usbsts = *(volatile u32*)((u64)Xhci->Op + 0x04);
    if (usbsts & 1) {
        DebugStr("Controller halted, restarting...\n");
        u32 usbcmd = *(volatile u32*)((u64)Xhci->Op + 0x00);
        usbcmd |= 1;
        *(volatile u32*)((u64)Xhci->Op + 0x00) = usbcmd;
        u32 timeout = 1000000;
        while ((*(volatile u32*)((u64)Xhci->Op + 0x04) & 1) && timeout--) {
            __asm__ volatile("pause");
        }
        if (timeout == 0) {
            DebugStr("Restart timeout!\n");
            return 0xFF;
        }
    }

    XhciTrb ev;
    DebugStr("Draining old events...\n");
    while (XhciReadEvent(Xhci, &ev)) {}

    u32 enqueue = ring->Enqueue;
    u32 cycle = ring->Cycle;
    u32 size = ring->RingSize;
    XhciTrb* trb;

    DebugStr("ControlTransfer: enqueue="); DebugU32(enqueue); DebugStr(" cycle="); DebugU32(cycle); DebugStr("\n");

    #define ADD_TRB_SIMPLE(low, high, status, ctrl) do { \
        if (enqueue == size - 1) { \
            XhciTrb* link = &ring->Ring[enqueue]; \
            link->ParameterLow = (u32)((u64)ring->Ring & 0xFFFFFFFF); \
            link->ParameterHigh = (u32)((u64)ring->Ring >> 32); \
            link->Status = 0; \
            link->Control = (6 << 10) | (1 << 1) | cycle; \
            FlushCache(link, sizeof(XhciTrb)); \
            enqueue = 0; \
            cycle ^= 1; \
        } \
        trb = &ring->Ring[enqueue]; \
        trb->ParameterLow = low; \
        trb->ParameterHigh = high; \
        trb->Status = status; \
        trb->Control = ctrl | cycle; \
        FlushCache(trb, sizeof(XhciTrb)); \
        enqueue++; \
    } while(0)

    u32 trt = (wLength > 0) ? ((bmRequestType & 0x80) ? 2 : 1) : 0;
    
    ADD_TRB_SIMPLE(
        ((u32)wValue << 16) | ((u32)bRequest << 8) | bmRequestType,
        ((u32)wLength << 16) | (u32)wIndex,
        (8 << 22), 
        (XHCI_TRB_SETUP_STAGE << 10) | (trt << 16) | (1 << 6)
    );

    if (wLength > 0 && Buffer) {
        FlushCache(Buffer, wLength);
        u32 dir = (bmRequestType & 0x80) ? (1 << 16) : 0;
        ADD_TRB_SIMPLE(
            (u32)((u64)Buffer & 0xFFFFFFFF),
            (u32)((u64)Buffer >> 32),
            wLength,
            (XHCI_TRB_DATA_STAGE << 10) | dir | XHCI_TRB_IOC
        );
    }

    u32 statusDir = 0;
    if (wLength == 0) statusDir = (1 << 16);
    else statusDir = (bmRequestType & 0x80) ? 0 : (1 << 16);
    
    ADD_TRB_SIMPLE(0, 0, 0, (XHCI_TRB_STATUS_STAGE << 10) | statusDir | XHCI_TRB_IOC);

    #undef ADD_TRB_SIMPLE

    ring->Enqueue = enqueue;
    ring->Cycle = cycle;
    DebugStr("After adding TRBs: enqueue="); DebugU32(enqueue); DebugStr(" cycle="); DebugU32(cycle); DebugStr("\n");

    FlushCache(ring->Ring, sizeof(XhciTrb) * ring->RingSize);

    XhciRingDoorbell(Xhci, SlotId, 1);

    usbsts = *(volatile u32*)((u64)Xhci->Op + 0x04);
    DebugStr("USBSTS after doorbell: 0x"); DebugU32(usbsts); DebugStr("\n");

    u32 timeout = 1000000;
    while (timeout--) {
        if (XhciReadEvent(Xhci, &ev)) {
            u8 type = (ev.Control >> 10) & 0x3F;
            u8 slot = (ev.Control >> 24) & 0xFF;
            u8 code = (ev.Status >> 24) & 0xFF;
            DebugStr("Event Type="); DebugU8(type); DebugStr(" Slot="); DebugU8(slot); DebugStr(" Code=0x"); DebugU8(code); DebugStr("\n");
            if (type == XHCI_TRB_TRANSFER_EVENT && slot == SlotId) {
                return code;
            }
        }
        __asm__ volatile("pause");
    }
    DebugStr("ControlTransfer Timeout!\n");
    return 0xFF;
}

void XhciGetDeviceDescriptor(XhciController* Xhci, u8 SlotId, UsbDeviceDescriptor* Desc) {
    DebugStr("XhciGetDeviceDescriptor: SlotId="); DebugU8(SlotId); DebugStr("\n");
    XhciControlTransfer(Xhci, SlotId, 0x80, USB_REQ_GET_DESCRIPTOR,
                        (USB_DT_DEVICE << 8) | 0, 0, sizeof(UsbDeviceDescriptor), Desc);
    DebugStr("Device Descriptor: bLength="); DebugU8(Desc->bLength);
    DebugStr(" bDescriptorType="); DebugU8(Desc->bDescriptorType);
    DebugStr(" bcdUSB=0x"); DebugU16(Desc->bcdUSB);
    DebugStr(" idVendor=0x"); DebugU16(Desc->idVendor);
    DebugStr(" idProduct=0x"); DebugU16(Desc->idProduct);
    DebugStr("\n");
}

void XhciParseDevice(XhciController* Xhci, u8 SlotId) {
    DebugStr("XhciParseDevice: SlotId="); DebugU8(SlotId); DebugStr("\n");
    
    UsbDeviceDescriptor devDesc;
    XhciGetDeviceDescriptor(Xhci, SlotId, &devDesc);

    u8 configHeader[9];
    u8 code = XhciControlTransfer(Xhci, SlotId, 0x80, USB_REQ_GET_DESCRIPTOR,
                        (USB_DT_CONFIG << 8) | 0, 0, 9, configHeader);
    DebugStr("Config Header transfer code=0x"); DebugU8(code); DebugStr("\n");
    if (code != XHCI_CMPLT_SUCCESS) {
        DebugStr("Get Config Header failed!\n");
        return;
    }

    u16 totalLen = *(u16*)(configHeader + 2);
    DebugStr("Total Config Length="); DebugU16(totalLen); DebugStr("\n");
    
    u8* configDesc = (u8*)Alloc(&KernelPool, totalLen);
    if (!configDesc) {
        DebugStr("Alloc configDesc failed!\n");
        return;
    }

    code = XhciControlTransfer(Xhci, SlotId, 0x80, USB_REQ_GET_DESCRIPTOR,
                        (USB_DT_CONFIG << 8) | 0, 0, totalLen, configDesc);
    DebugStr("Config transfer code=0x"); DebugU8(code); DebugStr("\n");
    if (code != XHCI_CMPLT_SUCCESS) {
        DebugStr("Get Config failed!\n");
        Free(&KernelPool, configDesc);
        return;
    }

    u8* ptr = configDesc;
    u8* end = ptr + totalLen;

    while (ptr < end) {
        u8 descLen = ptr[0];
        u8 descType = ptr[1];

        DebugStr("Descriptor: len="); DebugU8(descLen);
        DebugStr(" type=0x"); DebugU8(descType); DebugStr("\n");

        if (descType == USB_DT_INTERFACE) {
            UsbInterfaceDescriptor* iface = (UsbInterfaceDescriptor*)ptr;
            DebugStr("Interface: class=0x"); DebugU8(iface->bInterfaceClass);
            DebugStr(" subclass=0x"); DebugU8(iface->bInterfaceSubClass);
            DebugStr(" protocol=0x"); DebugU8(iface->bInterfaceProtocol); DebugStr("\n");

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
    if (!Xhci) {
        DebugStr("XhciPollEvents: Xhci is NULL!\n");
        return;
    }

    u32 usbsts = *(volatile u32*)((u64)Xhci->Op + 0x04);
    if (usbsts & 0x0C) {
        DebugStr("USBSTS Error: "); DebugU32(usbsts); DebugStr("\n");
    }
    XhciTrb Event;
    u32 event_count = 0;
    const u32 MAX_EVENTS_PER_POLL = 16;
    while (event_count < MAX_EVENTS_PER_POLL && XhciReadEvent(Xhci, &Event)) {
        event_count++;
        u8 Type = (Event.Control >> 10) & 0x3F;
        u8 SlotId = (Event.Control >> 24) & 0xFF;
        u8 CompletionCode = (Event.Status >> 24) & 0xFF;

        DebugStr("XhciPollEvents: Type="); DebugU8(Type);
        DebugStr(" SlotId="); DebugU8(SlotId);
        DebugStr(" CompletionCode=0x"); DebugU8(CompletionCode); DebugStr("\n");

        switch (Type) {
            case XHCI_TRB_TRANSFER_EVENT: {
                if (CompletionCode != XHCI_CMPLT_SUCCESS) continue;

                if (ActiveMouse && ((u64)ActiveMouse < 0x1000)) {
                    DebugStr("ActiveMouse is an invalid pointer!\n");
                    continue;
                }
                if (ActiveKbd && ((u64)ActiveKbd < 0x1000)) {
                    DebugStr("ActiveKbd is an invalid pointer!\n");
                    continue;
                }

                if (ActiveMouse && SlotId == ActiveMouse->SlotId) {
                    u8* report = ActiveMouse->ReportBuffer;
                    if (!report) continue;

                    ActiveMouse->Buttons = report[0] & 0x07;
                    ActiveMouse->Buffer[0] = (s8)report[1];
                    ActiveMouse->Buffer[1] = (s8)report[2];
                    ActiveMouse->Buffer[2] = report[0] & 0x07;

                    MousePrepareTransfer(ActiveMouse);
                    XhciRingDoorbell(Xhci, SlotId, (ActiveMouse->EpIn * 2) + 1);
                }
                else if (ActiveKbd && SlotId == ActiveKbd->SlotId) {
                    u8* report = ActiveKbd->ReportBuffer;
                    if (!report) continue;

                    if (report[2]) {
                        u8 scancode = report[2];
                        KeyboardPushScancode(ActiveKbd, scancode);
                    }
                    KeyboardQueueTransfer(Xhci, ActiveKbd);
                }
                break;
            }

            case XHCI_TRB_PORT_STATUS_CHANGE: {
                u32 PortId = (Event.ParameterLow >> 24) & 0xFF;
                DebugStr("Port Status Change: PortId="); DebugU32(PortId); DebugStr("\n");
                if (PortId == 0 || PortId > XHCI_MAX_PORTS) {
                    DebugStr("Invalid PortId: ");
                    DebugU32(PortId);
                    DebugStr("\n");
                    break;
                }
                XhciPortRegs* Port = &Xhci->Ports[PortId - 1];
                if (!Port) break;
                u32 Portsc = Port->Portsc;
                Port->Portsc = Portsc;
                if (Portsc & XHCI_PORTSC_CCS) {
                    u8 Speed = (Portsc >> 10) & 0x7;
                    XhciEnumerateDevice(Xhci, PortId, Speed);
                }
                break;
            }
        }
    }
}