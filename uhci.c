#include "uhci.h"
#include "str.h"
#include "port.h"

UHCIHostController* UHCICreate(u8 Bus, u8 Slot, u8 Func, AllocPool* Pool)
{
    UHCIHostController* hc = (UHCIHostController*)Alloc(Pool, sizeof(UHCIHostController));
    if (!hc) return NULL;
    MemSet(hc, 0, sizeof(UHCIHostController));

    hc->Bus = Bus;
    hc->Slot = Slot;
    hc->Func = Func;
    hc->MemoryPool = Pool;
    hc->PortCount = UHCI_PORT_COUNT;

    PCIEnableDevice(Bus, Slot, Func);

    u32 bar = PCIReadDWORD(Bus, Slot, Func, 0x10);
    DebugStr("PCI BAR0 raw = 0x");
    DebugU32(bar);
    DebugStr("\r\n");

    hc->IOBase = (u16)(bar & 0xFFFC);
    if (hc->IOBase == 0 || hc->IOBase < 0x1000) {
        DebugStr("BAR0 invalid, trying legacy I/O probe...\r\n");
        u16 possibleBases[] = {0xE000, 0xE020, 0xD000, 0xC000, 0xB000, 0xA000};
        for (int i = 0; i < 6; i++) {
            u16 test = possibleBases[i];
            outw(test + UHCI_CMD_OFFSET, 0x0000);
            SystemBusySleepMs(5);
            u16 readback = inw(test + UHCI_CMD_OFFSET);
            if (readback != 0xFFFF) {
                hc->IOBase = test;
                DebugStr("Found possible IOBase at 0x");
                DebugU16(hc->IOBase);
                DebugStr("\r\n");
                break;
            }
        }
    }

    DebugStr("Final IOBase = 0x");
    DebugU16(hc->IOBase);
    DebugStr("\r\n");

    if (hc->IOBase == 0) {
        DebugStr("CRITICAL: Still cannot get valid IOBase!\r\n");
    }

    u32 cmd = PCIReadDWORD(Bus, Slot, Func, 0x04);
    PCIWriteDWORD(Bus, Slot, Func, 0x04, cmd | 0x07);

    return hc;
}

UHCIResult UHCIInitialize(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    if (ctx->HC->IOBase == 0) {
        DebugStr("FATAL: IOBase is 0, cannot initialize UHCI!\r\n");
        return UHCI_ERROR_GENERAL;
    }
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    ctx->FrameListSize = UHCI_FRAME_LIST_SIZE;
    ctx->MaxRequests = UHCI_MAX_REQUESTS;
    ctx->Initialized = 0;
    ctx->PollMode = 1;
    ctx->FrameList = (UHCIFrameListEntry*)AlignedAlloc(ctx->HC->MemoryPool, UHCI_FRAME_LIST_BYTES, 4096);
    if (!ctx->FrameList) return UHCI_ERROR_NO_MEMORY;
    MemSet(ctx->FrameList, 0, UHCI_FRAME_LIST_BYTES);
    ctx->FrameListPhys = (u32)((u64)ctx->FrameList & 0xFFFFFFFF);
    for (u32 i = 0; i < UHCI_FRAME_LIST_SIZE; i++) {
        ctx->FrameList[i].Pointer = UHCI_FRAME_TERMINATE;
    }
    UHCIResult res = UHCIReset(ctx);
    if (res != UHCI_OK) return res;
    outl(ctx->HC->IOBase + UHCI_FRBASE_OFFSET, ctx->FrameListPhys);
    outw(ctx->HC->IOBase + UHCI_FRNUM_OFFSET, 0);
    outw(ctx->HC->IOBase + UHCI_INTR_OFFSET, 0);
    ctx->Initialized = 1;
    return UHCI_OK;
}

UHCIResult UHCIReset(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    u16 io = ctx->HC->IOBase;
    DebugStr("UHCIReset: IOBase=0x");
    DebugU16(io);
    DebugStr("\r\n");

    outw(io + UHCI_CMD_OFFSET, inw(io + UHCI_CMD_OFFSET) & ~UHCI_CMD_RS);
    SystemBusySleepMs(10);

    outw(io + UHCI_CMD_OFFSET, UHCI_CMD_HCRESET);
    DebugStr("HCRESET command sent\r\n");

    int timeout = 10000;
    while (inw(io + UHCI_CMD_OFFSET) & UHCI_CMD_HCRESET) {
        if (--timeout <= 0) {
            DebugStr("UHCIReset TIMEOUT!\r\n");
            return UHCI_ERROR_TIMEOUT;
        }
        SystemBusySleepMs(1);
    }
    DebugStr("HCRESET completed\r\n");

    PCIWriteDWORD(ctx->HC->Bus, ctx->HC->Slot, ctx->HC->Func, 0xC0, 0x2000);
    DebugStr("PCI config 0xC0 written\r\n");
    return UHCI_OK;
}

UHCIResult UHCIConfigure(UHCIContext* ctx, u8 maxPacket, u8 debugMode)
{
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    ctx->HC->MaxPacket = maxPacket;
    ctx->HC->DebugMode = debugMode;
    u16 io = ctx->HC->IOBase;
    u16 cmd = inw(io + UHCI_CMD_OFFSET);
    if (maxPacket == 64) {
        cmd |= UHCI_CMD_MAXP;
    } else {
        cmd &= ~UHCI_CMD_MAXP;
    }
    if (debugMode) {
        cmd |= UHCI_CMD_SWDBG;
    } else {
        cmd &= ~UHCI_CMD_SWDBG;
    }
    outw(io + UHCI_CMD_OFFSET, cmd);
    ctx->HC->Configured = 1;
    return UHCI_OK;
}

UHCIResult UHCIStart(UHCIContext* ctx)
{
    if (!ctx || !ctx->Initialized) return UHCI_ERROR_GENERAL;
    u16 io = ctx->HC->IOBase;
    u16 cmd = inw(io + UHCI_CMD_OFFSET);
    cmd |= UHCI_CMD_RS | UHCI_CMD_CF | UHCI_CMD_MAXP;
    outw(io + UHCI_CMD_OFFSET, cmd);
    ctx->HC->Running = 1;
    SystemBusySleepMs(10);
    return UHCI_OK;
}

UHCIResult UHCIStop(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    u16 io = ctx->HC->IOBase;
    outw(io + UHCI_CMD_OFFSET, inw(io + UHCI_CMD_OFFSET) & ~UHCI_CMD_RS);
    ctx->HC->Running = 0;
    return UHCI_OK;
}

u8 UHCIIsHalted(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) return 1;
    return (inw(ctx->HC->IOBase + UHCI_STS_OFFSET) & UHCI_STS_HCHALTED) ? 1 : 0;
}

u8 UHCIIsRunning(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) return 0;
    return (inw(ctx->HC->IOBase + UHCI_CMD_OFFSET) & UHCI_CMD_RS) ? 1 : 0;
}

u16 UHCIGetFrameNumber(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) return 0;
    return inw(ctx->HC->IOBase + UHCI_FRNUM_OFFSET);
}

UHCIResult UHCISetFrameNumber(UHCIContext* ctx, u16 Frame)
{
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    outw(ctx->HC->IOBase + UHCI_FRNUM_OFFSET, Frame & 0x3FF);
    return UHCI_OK;
}

u16 UHCIGetPortStatus(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return 0;
    return inw(ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2));
}

UHCIResult UHCIResetPort(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 portRegister = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    outw(portRegister, UHCI_PORTSC_PR);
    SystemBusySleepMs(UHCI_PORT_RESET_DELAY_MS);
    outw(portRegister, inw(portRegister) & ~UHCI_PORTSC_PR);
    SystemBusySleepMs(10);
    int timeout = UHCI_POLL_TIMEOUT;
    while (1) {
        u16 status = inw(portRegister);
        if (status & UHCI_PORTSC_CSC) {
            outw(portRegister, status | UHCI_PORTSC_CSC);
            continue;
        }
        if (status & UHCI_PORTSC_PE) break;
        outw(portRegister, status | UHCI_PORTSC_PE);
        if (--timeout <= 0) return UHCI_ERROR_TIMEOUT;
        SystemBusySleepMs(1);
    }
    return UHCI_OK;
}

UHCIResult UHCIEnablePort(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    u16 val = inw(reg);
    outw(reg, (val & ~UHCI_PORTSC_W1C) | UHCI_PORTSC_PE);
    return UHCI_OK;
}

UHCIResult UHCIDisablePort(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    u16 val = inw(reg);
    outw(reg, (val & ~UHCI_PORTSC_W1C) & ~UHCI_PORTSC_PE);
    return UHCI_OK;
}

UHCIResult UHCISuspendPort(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    u16 val = inw(reg);
    outw(reg, (val & ~UHCI_PORTSC_W1C) | UHCI_PORTSC_SUSP);
    return UHCI_OK;
}

UHCIResult UHCIResumePort(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    u16 val = inw(reg);
    outw(reg, (val & ~UHCI_PORTSC_W1C) | UHCI_PORTSC_RD);
    SystemBusySleepMs(UHCI_RESUME_DELAY_MS);
    outw(reg, inw(reg) & ~UHCI_PORTSC_RD);
    return UHCI_OK;
}

UHCIResult UHCIClearPortChange(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    u16 val = inw(reg);
    outw(reg, val | UHCI_PORTSC_W1C);
    return UHCI_OK;
}

UHCIRequest* UHCICreateRequest(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) return NULL;
    UHCIRequest* req = (UHCIRequest*)Alloc(ctx->HC->MemoryPool, sizeof(UHCIRequest));
    if (!req) return NULL;
    MemSet(req, 0, sizeof(UHCIRequest));
    req->UHCI = ctx;
    return req;
}

static void FreeTDChain(UHCIContext* ctx, UHCITransferDescriptor* td)
{
    while (td) {
        u32 next = td->LinkPointer & UHCI_FRAME_PTR_MASK;
        if (next == UHCI_FRAME_TERMINATE || next == 0) break;
        UHCITransferDescriptor* nextTD = (UHCITransferDescriptor*)(u64)next;
        td = nextTD;
    }
}

UHCIResult UHCISubmitRequest(UHCIContext* ctx, UHCIRequest* Req)
{
    if (!ctx || !Req || !ctx->HC) return UHCI_ERROR_GENERAL;
    if (Req->Type != UHCI_TRANSFER_CONTROL) {
        return UHCI_ERROR_GENERAL;
    }
    UHCITransferDescriptor* setupTD = (UHCITransferDescriptor*)AlignedAlloc(ctx->HC->MemoryPool, sizeof(UHCITransferDescriptor), 16);
    UHCITransferDescriptor* dataTD = NULL;
    UHCITransferDescriptor* statusTD = (UHCITransferDescriptor*)AlignedAlloc(ctx->HC->MemoryPool, sizeof(UHCITransferDescriptor), 16);
    if (!setupTD || !statusTD) {
        if (setupTD) /* free logic omitted for brevity, add proper free */;
        return UHCI_ERROR_NO_MEMORY;
    }
    MemSet(setupTD, 0, sizeof(UHCITransferDescriptor));
    MemSet(statusTD, 0, sizeof(UHCITransferDescriptor));
    u32 speedBit = (UHCIGetPortStatus(ctx, 0) & UHCI_PORTSC_LSDA) ? (1u << 26) : 0;
    setupTD->ControlStatus = UHCI_TD_ACTIVE | (3u << 27) | speedBit | UHCI_TD_IOC;
    setupTD->Token = (7u << 21) | (Req->Endpoint << 15) | (Req->DeviceAddress << 8) | UHCI_TD_PID_SETUP;
    setupTD->BufferPointer = (u32)((u64)Req->DataBuffer & 0xFFFFFFFF);
    if (Req->DataLength > 0) {
        dataTD = (UHCITransferDescriptor*)AlignedAlloc(ctx->HC->MemoryPool, sizeof(UHCITransferDescriptor), 16);
        if (!dataTD) {
            /* free */;
            return UHCI_ERROR_NO_MEMORY;
        }
        MemSet(dataTD, 0, sizeof(UHCITransferDescriptor));
        dataTD->ControlStatus = UHCI_TD_ACTIVE | (3u << 27) | speedBit | UHCI_TD_IOC;
        u32 pid = (Req->Direction == 1) ? UHCI_TD_PID_IN : UHCI_TD_PID_OUT;
        dataTD->Token = ((Req->DataLength - 1) << 21) | (1u << 19) | (Req->Endpoint << 15) | (Req->DeviceAddress << 8) | pid;
        dataTD->BufferPointer = (u32)((u64)Req->DataBuffer & 0xFFFFFFFF);
    }
    statusTD->ControlStatus = UHCI_TD_ACTIVE | (3u << 27) | speedBit | UHCI_TD_IOC;
    statusTD->Token = (0u << 21) | (0u << 19) | (Req->Endpoint << 15) | (Req->DeviceAddress << 8) | UHCI_TD_PID_OUT;
    statusTD->BufferPointer = 0;
    setupTD->LinkPointer = (u32)(((u64)dataTD ? (u64)dataTD : (u64)statusTD) & 0xFFFFFFFF) | 0x04;
    if (dataTD) {
        dataTD->LinkPointer = (u32)(((u64)statusTD & 0xFFFFFFFF) | 0x04);
    }
    statusTD->LinkPointer = UHCI_FRAME_TERMINATE;
    Req->QH = (UHCIQueueHead*)AlignedAlloc(ctx->HC->MemoryPool, sizeof(UHCIQueueHead), 16);
    if (!Req->QH) {
        /* free TDs */;
        return UHCI_ERROR_NO_MEMORY;
    }
    MemSet(Req->QH, 0, sizeof(UHCIQueueHead));
    Req->QH->HorizontalLink = UHCI_FRAME_TERMINATE;
    Req->QH->VerticalLink = (u32)(((u64)setupTD & 0xFFFFFFFF) | UHCI_QH_VERTICAL);
    Req->TD = setupTD;
    Req->TDList = dataTD ? dataTD : statusTD;
    Req->Completed = 0;
    Req->Active = 1;
    Req->Next = NULL;
    if (!ctx->PendingRequests) {
        ctx->PendingRequests = Req;
    } else {
        UHCIRequest* last = ctx->PendingRequests;
        while (last->Next) last = last->Next;
        last->Next = Req;
    }
    u16 FrameNum = UHCIGetFrameNumber(ctx);
    u16 FrameIdx = FrameNum & (UHCI_FRAME_LIST_SIZE - 1);
    ctx->FrameList[FrameIdx].Pointer = (u32)(((u64)Req->QH & 0xFFFFFFFF) | UHCI_FRAME_QH);
    return UHCI_OK;
}

UHCIResult UHCICancelRequest(UHCIContext* ctx, UHCIRequest* req)
{
    if (!ctx || !req) return UHCI_ERROR_GENERAL;
    UHCIRequest* Cur = ctx->PendingRequests;
    UHCIRequest* Prev = NULL;
    while (Cur) {
        if (Cur == req) {
            for (int i = 0; i < UHCI_FRAME_LIST_SIZE; i++) {
                if ((ctx->FrameList[i].Pointer & ~0xF) == ((u32)((u64)req->QH & 0xFFFFFFFF) & ~0xF)) {
                    ctx->FrameList[i].Pointer = UHCI_FRAME_TERMINATE;
                }
            }
            if (Prev) {
                Prev->Next = Cur->Next;
            } else {
                ctx->PendingRequests = Cur->Next;
            }
            req->Active = 0;
            req->Completed = 1;
            req->Result = UHCI_ERROR_GENERAL;
            if (req->Callback) req->Callback(req);
            return UHCI_OK;
        }
        Prev = Cur;
        Cur = Cur->Next;
    }
    return UHCI_ERROR_GENERAL;
}

static UHCIResult UHCICheckRequestStatus(UHCIRequest* Req)
{
    UHCITransferDescriptor* td = Req->TD;
    u16 actual = 0;
    while (td) {
        u32 cs = td->ControlStatus;
        if (cs & UHCI_TD_ACTIVE) {
            return UHCI_OK;
        }
        if (cs & UHCI_TD_STALL) {
            Req->Completed = 1;
            Req->Stalled = 1;
            return UHCI_ERROR_STALL;
        }
        if (cs & (UHCI_TD_CRCERR | UHCI_TD_BITSTUFF | UHCI_TD_BABBLE | UHCI_TD_DBUFERR)) {
            Req->Completed = 1;
            return UHCI_ERROR_GENERAL;
        }
        actual += (cs & UHCI_TD_ACTLEN_MASK);
        if (td->LinkPointer & UHCI_FRAME_TERMINATE) break;
        td = (UHCITransferDescriptor*)(u64)(td->LinkPointer & UHCI_FRAME_PTR_MASK);
    }
    Req->Completed = 1;
    Req->Success = 1;
    Req->ActualLength = actual;
    return UHCI_OK;
}

UHCIResult UHCIPoll(UHCIContext* ctx)
{
    if (!ctx) return UHCI_ERROR_GENERAL;
    UHCIRequest* Cur = ctx->PendingRequests;
    UHCIRequest* Prev = NULL;
    while (Cur) {
        UHCIRequest* Next = Cur->Next;
        UHCIResult Result = UHCICheckRequestStatus(Cur);
        if (Result != UHCI_OK || Cur->Completed) {
            for (int i = 0; i < UHCI_FRAME_LIST_SIZE; i++) {
                if ((ctx->FrameList[i].Pointer & ~0xF) == ((u32)((u64)Cur->QH & 0xFFFFFFFF) & ~0xF)) {
                    ctx->FrameList[i].Pointer = UHCI_FRAME_TERMINATE;
                }
            }
            if (Prev) {
                Prev->Next = Next;
            } else {
                ctx->PendingRequests = Next;
            }
            if (ctx->CompletedRequests) {
                UHCIRequest* tail = ctx->CompletedRequests;
                while (tail->Next) tail = tail->Next;
                tail->Next = Cur;
            } else {
                ctx->CompletedRequests = Cur;
            }
            if (Cur->Callback) {
                Cur->Callback(Cur);
            }
            Cur->Next = NULL;
        } else {
            Prev = Cur;
        }
        Cur = Next;
    }
    u16 io = ctx->HC->IOBase;
    u16 sts = inw(io + UHCI_STS_OFFSET);
    if (sts) outw(io + UHCI_STS_OFFSET, sts);
    return UHCI_OK;
}

void UHCIDumpStatus(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) return;
    u16 io = ctx->HC->IOBase;
    u16 cmd = inw(io + UHCI_CMD_OFFSET);
    u16 sts = inw(io + UHCI_STS_OFFSET);
    u16 intr = inw(io + UHCI_INTR_OFFSET);
    u16 frnum = inw(io + UHCI_FRNUM_OFFSET);
    u32 frbase = inl(io + UHCI_FRBASE_OFFSET);
    DebugStr("UHCI IOBase: 0x");
    DebugU16(io);
    DebugChar('\n');
    DebugStr("CMD: 0x");
    DebugU16(cmd);
    DebugStr(", STS: 0x");
    DebugU16(sts);
    DebugChar('\n');
    DebugStr("INTR: 0x");
    DebugU16(intr);
    DebugStr(", FRNUM: 0x");
    DebugU16(frnum);
    DebugChar('\n');
    DebugStr("FRBASE: 0x");
    DebugU32(frbase);
    DebugChar('\n');
    for(u8 i = 0; i < ctx->HC->PortCount; i++) {
        u16 psc = inw(io + UHCI_PORTSC1_OFFSET + (i * 2));
        DebugStr("PORTSC");
        DebugU8(i + 1);
        DebugStr(": 0x");
        DebugU16(psc);
        DebugChar('\n');
    }
}

void UHCIDumpRequests(UHCIContext* ctx)
{
    if (!ctx) return;
    DebugStr("--- Pending Requests ---\n");
    UHCIRequest* curr = ctx->PendingRequests;
    while(curr) {
        DebugStr("Req [Dev: ");
        DebugU8(curr->DeviceAddress);
        DebugStr(", EP: ");
        DebugU8(curr->Endpoint);
        DebugStr("] Act: ");
        DebugU8(curr->Active);
        DebugStr(", Cmp: ");
        DebugU8(curr->Completed);
        DebugChar('\n');
        curr = curr->Next;
    }
    DebugStr("--- Completed Requests ---\n");
    curr = ctx->CompletedRequests;
    while(curr) {
        DebugStr("Req [Dev: ");
        DebugU8(curr->DeviceAddress);
        DebugStr(", EP: ");
        DebugU8(curr->Endpoint);
        DebugStr("] Success: ");
        DebugU8(curr->Success);
        DebugChar('\n');
        curr = curr->Next;
    }
}

void UHCIDumpFrameList(UHCIContext* ctx, u16 start, u16 count)
{
    if (!ctx || !ctx->FrameList || start >= UHCI_FRAME_LIST_SIZE) return;
    u16 end = start + count;
    if (end > UHCI_FRAME_LIST_SIZE) end = UHCI_FRAME_LIST_SIZE;
    DebugStr("--- FrameList Dump ---\n");
    for(u16 i = start; i < end; i++) {
        u32 val = ctx->FrameList[i].Pointer;
        if (val != UHCI_FRAME_TERMINATE) {
            DebugStr("Frame [");
            DebugU16(i);
            DebugStr("]: 0x");
            DebugU32(val);
            DebugChar('\n');
        }
    }
}