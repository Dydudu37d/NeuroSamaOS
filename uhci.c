#include "uhci.h"
#include "str.h"
#include "port.h"

static void* UHCIMemAlloc(AllocPool* Pool, size_t size) {
    return MaxAlloc(Pool, size, UHCI_MAX_ADDRESS);
}

static void* UHCIMemAlignedAlloc(AllocPool* Pool, size_t size, size_t align) {
    return MaxAlignedAlloc(Pool, size, align, UHCI_MAX_ADDRESS);
}

static void UHCISafeWritePort(u16 reg, u16 bits_to_set, u16 bits_to_clear) {
    u16 val = inw(reg);
    val &= ~UHCI_PORTSC_W1C;
    val |= bits_to_set;
    val &= ~bits_to_clear;
    outw(reg, val);
}

UHCIResult ExecuteControlTransfer(UHCIContext *ctx, u8 devAddr, u8 endpoint,
    u8 *setupPacket, u16 setupLen, u8 *data, u16 dataLen, u8 dir, u8 is_low_speed, u16 maxPacketSize) {

    DebugStr("Control Transfer Start: dev="); DebugU8(devAddr);
    DebugStr(" ep="); DebugU8(endpoint);
    DebugStr(" dir="); DebugU8(dir);
    DebugStr(" len="); DebugU16(dataLen);
    DebugStr(" lowspeed="); DebugU8(is_low_speed);
    DebugChar('\n');

    if (!ctx || !setupPacket || setupLen > 16) {
        DebugStr("Invalid param\n");
        return UHCI_ERROR_GENERAL;
    }
    if (maxPacketSize == 0) maxPacketSize = 8;
    if (dataLen > maxPacketSize * 8) {
        DebugStr("Data too long\n");
        return UHCI_ERROR_GENERAL;
    }

    int dataTdCount = 0;
    if (dataLen > 0 && data) {
        dataTdCount = (dataLen + maxPacketSize - 1) / maxPacketSize;
    }
    int totalTds = 1 + dataTdCount + 1;

    u32 qSize = sizeof(UHCIQueueHead) + totalTds * sizeof(UHCITransferDescriptor) + 32 + dataLen;
    void *q = MaxAlignedAlloc(ctx->MemoryPool, qSize, 16, 0xFFFFFFFFULL);
    if (!q) {
        DebugStr("Alloc failed\n");
        return UHCI_ERROR_NO_MEMORY;
    }

    MemSet(q, 0, qSize);
    UHCIQueueHead *qh = (UHCIQueueHead*)q;
    volatile UHCITransferDescriptor *tds = (volatile UHCITransferDescriptor*)((u8*)q + sizeof(UHCIQueueHead));
    u8 *setupBuffer = (u8*)((u8*)tds + totalTds * sizeof(UHCITransferDescriptor));
    u8 *dataBuffer = (u8*)(setupBuffer + 32);

    u32 qhPhys = (u32)(u64)qh;
    u32 tdPhys = (u32)(u64)tds;

    MemCopy(setupBuffer, setupPacket, setupLen);
    if (dataLen > 0 && data && dir == 0) {
        MemCopy(dataBuffer, data, dataLen);
    }

    u32 ctrl_ls = is_low_speed ? UHCI_TD_SPD : 0;

    tds[0].ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((setupLen - 1) & 0x7FF);
    tds[0].Token = UHCI_TD_PID_SETUP | ((devAddr & 0x7F) << 8) | ((endpoint & 0xF) << 15) | (((setupLen - 1) & 0x7FF) << 21);
    tds[0].BufferPointer = (u32)(u64)setupBuffer;

    if (dataLen > 0 && data) {
        u16 remaining = dataLen;
        u16 offset = 0;
        u8 toggle = 1;
        tds[0].LinkPointer = (tdPhys + sizeof(UHCITransferDescriptor)) & ~0xF;

        for (int i = 0; i < dataTdCount; i++) {
            u16 chunkSize = (remaining > maxPacketSize) ? maxPacketSize : remaining;
            int tdIdx = 1 + i;
            tds[tdIdx].ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((chunkSize - 1) & 0x7FF);
            tds[tdIdx].Token = (dir == 0 ? UHCI_TD_PID_OUT : UHCI_TD_PID_IN)
                | ((devAddr & 0x7F) << 8)
                | ((endpoint & 0xF) << 15)
                | (((chunkSize - 1) & 0x7FF) << 21)
                | (toggle << 19);
            tds[tdIdx].BufferPointer = (u32)(u64)(dataBuffer + offset);
            tds[tdIdx].LinkPointer = (tdPhys + (tdIdx + 1) * sizeof(UHCITransferDescriptor)) & ~0xF;

            remaining -= chunkSize;
            offset += chunkSize;
            toggle ^= 1;
        }
    } else {
        tds[0].LinkPointer = (tdPhys + (totalTds - 1) * sizeof(UHCITransferDescriptor)) & ~0xF;
    }

    int statusIdx = totalTds - 1;
    tds[statusIdx].LinkPointer = UHCI_FRAME_TERMINATE;
    tds[statusIdx].ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | ctrl_ls | 0x7FF;

    u8 statusPid = (dir == 1) ? UHCI_TD_PID_OUT : UHCI_TD_PID_IN;
    if (dataLen == 0) statusPid = UHCI_TD_PID_IN;

    u8 statusToggle = 1;
    tds[statusIdx].Token = statusPid 
        | ((devAddr & 0x7F) << 8) 
        | ((endpoint & 0xF) << 15) 
        | (statusToggle << 19);
    tds[statusIdx].BufferPointer = 0;

    qh->HorizontalLink = UHCI_FRAME_TERMINATE;
    qh->VerticalLink = (tdPhys & ~0xF) | UHCI_QH_VERTICAL;

    u16 frameIndex = 0;
    u32 oldFrameHead = ctx->FrameList[frameIndex].Pointer;
    ctx->FrameList[frameIndex].Pointer = (qhPhys & ~0xF) | UHCI_FRAME_QH;
    
    __asm__ volatile("mfence" ::: "memory");
    DebugStr("QH linked to frame 0, QHPhys=0x"); DebugU32(qhPhys); DebugChar('\n');
    DebugStr("Starting transfer on frame "); DebugU16(frameIndex); DebugChar('\n');

    u32 timeout = 200000;
    UHCIResult result = UHCI_ERROR_TIMEOUT;

    while (timeout--) {
        UHCIPoll(ctx);

        u16 sts = inw(ctx->HC->IOBase + UHCI_STS_OFFSET);
        if (sts) {
            outw(ctx->HC->IOBase + UHCI_STS_OFFSET, sts);
        }
        
        __asm__ volatile("mfence" ::: "memory");
        u32 final_cs = tds[statusIdx].ControlStatus;
        
        if (!(final_cs & UHCI_TD_ACTIVE)) {
            DebugStr("Status TD done! CS=0x"); DebugU32(final_cs); DebugChar('\n');

            if (final_cs & UHCI_TD_STALL) {
                DebugStr(">>> STALL <<<\n");
                result = UHCI_ERROR_STALL;
            } else if (final_cs & (UHCI_TD_CRCERR | UHCI_TD_BITSTUFF | UHCI_TD_BABBLE | UHCI_TD_DBUFERR)) {
                DebugStr("Transfer Error\n");
                result = UHCI_ERROR_GENERAL;
            } else {
                if (dataLen > 0 && data && dir == 1) {
                    MemCopy(data, dataBuffer, dataLen);
                }
                result = UHCI_OK;
            }
            break;
        }

        for (int i = 0; i < 400; i++) __asm__ volatile("pause");
    }

    if (result == UHCI_ERROR_TIMEOUT) {
        DebugStr("TIMEOUT! Final CS=0x"); DebugU32(tds[statusIdx].ControlStatus); DebugChar('\n');
    }

    ctx->FrameList[frameIndex].Pointer = oldFrameHead;
    Free(ctx->MemoryPool, q);

    DebugStr("Control Transfer End, result="); 
    if (result == UHCI_OK) DebugStr("OK\n");
    else if (result == UHCI_ERROR_STALL) DebugStr("STALL\n");
    else DebugStr("ERROR\n");

    return result;
}

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

    u32 bar0 = PCIReadDWORD(Bus, Slot, Func, 0x10);
    u32 bar4 = PCIReadDWORD(Bus, Slot, Func, 0x20);

    DebugStr("BAR0=0x"); DebugU32(bar0); DebugStr(" BAR4=0x"); DebugU32(bar4); DebugChar('\n');

    hc->IOBase = 0;

    if ((bar4 & 1) && (bar4 & 0xFFFE) != 0) {
        hc->IOBase = bar4 & 0xFFFE;
    }
    else if ((bar0 & 1) && (bar0 & 0xFFFE) != 0) {
        hc->IOBase = bar0 & 0xFFFE;
    }
    else {
        return NULL;
    }

    return hc;
}

UHCIResult UHCIInitialize(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC || ctx->HC->IOBase == 0) {
        return UHCI_ERROR_GENERAL;
    }

    ctx->FrameListSize = UHCI_FRAME_LIST_SIZE;
    ctx->MaxRequests = UHCI_MAX_REQUESTS;
    ctx->Initialized = 0;
    ctx->PollMode = 1;
    ctx->IsPolling = 0;
    ctx->CompletedRequests = NULL;
    ctx->CompletedRequestsTail = NULL;
    ctx->PendingRequests = NULL;

    ctx->FrameList = (UHCIFrameListEntry*)MaxAlignedAlloc(ctx->HC->MemoryPool, UHCI_FRAME_LIST_BYTES, 4096, UHCI_MAX_ADDRESS);
    if (!ctx->FrameList) {
        return UHCI_ERROR_NO_MEMORY;
    }

    MemSet(ctx->FrameList, 0, UHCI_FRAME_LIST_BYTES);

    ctx->FrameListPhys = (u32)(u64)ctx->FrameList;
    ctx->FrameListVirt = (void*)ctx->FrameList;

    for (u32 i = 0; i < UHCI_FRAME_LIST_SIZE; i++) {
        ctx->FrameList[i].Pointer = UHCI_FRAME_TERMINATE;
    }

    UHCIResult res = UHCIReset(ctx);
    if (res != UHCI_OK) {
        return res;
    }

    outl(ctx->HC->IOBase + UHCI_FRBASE_OFFSET, ctx->FrameListPhys);
    outw(ctx->HC->IOBase + UHCI_FRNUM_OFFSET, 0);
    outw(ctx->HC->IOBase + UHCI_INTR_OFFSET, 0);

    ctx->Initialized = 1;
    return UHCI_OK;
}

UHCIResult UHCIReset(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) {
        return UHCI_ERROR_GENERAL;
    }
    u16 io = ctx->HC->IOBase;

    outw(io + UHCI_CMD_OFFSET, inw(io + UHCI_CMD_OFFSET) & ~UHCI_CMD_RS);
    SystemBusySleepMs(10);

    outw(io + UHCI_CMD_OFFSET, UHCI_CMD_HCRESET);

    int timeout = 10000;
    while (inw(io + UHCI_CMD_OFFSET) & UHCI_CMD_HCRESET) {
        if (--timeout <= 0) {
            return UHCI_ERROR_TIMEOUT;
        }
        SystemBusySleepMs(1);
    }

    timeout = 10000;
    while (!(inw(io + UHCI_STS_OFFSET) & UHCI_STS_HCHALTED)) {
        if (--timeout <= 0) {
            return UHCI_ERROR_TIMEOUT;
        }
        SystemBusySleepMs(1);
    }

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

    int timeout = 10000;
    while (inw(io + UHCI_STS_OFFSET) & UHCI_STS_HCHALTED) {
        if (--timeout <= 0) return UHCI_ERROR_TIMEOUT;
        SystemBusySleepMs(1);
    }

    ctx->HC->Running = 1;

    SystemBusySleepMs(20);
    outw(io + UHCI_FRNUM_OFFSET, 0);
    __asm__ volatile("mfence" ::: "memory");

    for (int i = 0; i < 16; i++) {
        ctx->FrameList[i].Pointer = ctx->FrameList[i].Pointer;
    }
    __asm__ volatile("mfence" ::: "memory");

    UHCIDumpFrameList(ctx, 0, 8);

    SystemBusySleepMs(50);
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

    UHCISafeWritePort(portRegister, UHCI_PORTSC_PR, 0);
    SystemBusySleepMs(UHCI_PORT_RESET_DELAY_MS);

    UHCISafeWritePort(portRegister, 0, UHCI_PORTSC_PR);
    SystemBusySleepMs(50);

    int timeout = UHCI_POLL_TIMEOUT;
    while (1) {
        u16 status = inw(portRegister);

        if (status & UHCI_PORTSC_CSC) {
            outw(portRegister, (status & ~UHCI_PORTSC_W1C) | UHCI_PORTSC_CSC);
            continue;
        }

        if (status & UHCI_PORTSC_PE) break;

        UHCISafeWritePort(portRegister, UHCI_PORTSC_PE, 0);

        if (--timeout <= 0) return UHCI_ERROR_TIMEOUT;
        SystemBusySleepMs(1);
    }
    return UHCI_OK;
}

UHCIResult UHCIEnablePort(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    UHCISafeWritePort(reg, UHCI_PORTSC_PE, 0);
    return UHCI_OK;
}

UHCIResult UHCIDisablePort(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    UHCISafeWritePort(reg, 0, UHCI_PORTSC_PE);
    return UHCI_OK;
}

UHCIResult UHCISuspendPort(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    UHCISafeWritePort(reg, UHCI_PORTSC_SUSP, 0);
    return UHCI_OK;
}

UHCIResult UHCIResumePort(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    UHCISafeWritePort(reg, UHCI_PORTSC_RD, 0);
    SystemBusySleepMs(UHCI_RESUME_DELAY_MS);
    UHCISafeWritePort(reg, 0, UHCI_PORTSC_RD);
    return UHCI_OK;
}

UHCIResult UHCIClearPortChange(UHCIContext* ctx, u8 Port)
{
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    u16 val = inw(reg);
    outw(reg, (val & ~UHCI_PORTSC_W1C) | UHCI_PORTSC_W1C);
    return UHCI_OK;
}

UHCIRequest* UHCICreateRequest(UHCIContext* ctx)
{
    if (!ctx || !ctx->HC) return NULL;
    UHCIRequest* req = (UHCIRequest*)UHCIMemAlloc(ctx->HC->MemoryPool, sizeof(UHCIRequest));
    if (!req) return NULL;
    MemSet(req, 0, sizeof(UHCIRequest));
    req->UHCI = ctx;
    return req;
}

UHCIResult UHCISubmitRequest(UHCIContext* ctx, UHCIRequest* Req)
{
    if (!ctx || !Req || !ctx->HC) return UHCI_ERROR_GENERAL;

    Req->Completed = 0;
    Req->Active = 1;
    Req->Success = 0;
    Req->Result = UHCI_OK;
    Req->Next = NULL;
    Req->ErrorCount = 0;

    if (!ctx->PendingRequests) {
        ctx->PendingRequests = Req;
    } else {
        UHCIRequest* last = ctx->PendingRequests;
        while (last->Next) last = last->Next;
        last->Next = Req;
    }

    u16 FrameNum = UHCIGetFrameNumber(ctx);
    Req->FrameIndex = FrameNum & (UHCI_FRAME_LIST_SIZE - 1);

    if (Req->QH && ctx->FrameList) {
        __asm__ volatile("" ::: "memory");
        ctx->FrameList[Req->FrameIndex].Pointer = Req->QHPhys | UHCI_FRAME_QH;
    }

    return UHCI_OK;
}

UHCIResult UHCICancelRequest(UHCIContext* ctx, UHCIRequest* req)
{
    if (!ctx || !req) return UHCI_ERROR_GENERAL;
    UHCIRequest* Cur = ctx->PendingRequests;
    UHCIRequest* Prev = NULL;
    while (Cur) {
        if (Cur == req) {
            if (req->QH && ctx->FrameList) {
                ctx->FrameList[req->FrameIndex].Pointer = UHCI_FRAME_TERMINATE;
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
    if (!Req || !Req->TD) {
        if (Req) {
            Req->Completed = 1;
            Req->Result = UHCI_ERROR_GENERAL;
        }
        return UHCI_ERROR_GENERAL;
    }

    __asm__ volatile("mfence" ::: "memory");

    UHCITransferDescriptor* td = Req->TD;
    u32 cs = td->ControlStatus;
    u16 actual = 0;

    if (cs & UHCI_TD_ACTIVE) {
        return UHCI_OK;
    }

    if (cs & UHCI_TD_STALL) {
        Req->Completed = 1;
        Req->Stalled = 1;
        Req->Result = UHCI_ERROR_STALL;
        return UHCI_ERROR_STALL;
    }

    if (cs & (UHCI_TD_CRCERR | UHCI_TD_BITSTUFF | UHCI_TD_BABBLE | UHCI_TD_DBUFERR)) {
        Req->Completed = 1;
        Req->Result = UHCI_ERROR_GENERAL;
        return UHCI_ERROR_GENERAL;
    }

    if (Req->Type == UHCI_TRANSFER_INTERRUPT) {
        Req->ActualLength = (cs & UHCI_TD_ACTLEN_MASK);
        Req->Completed = 1;
        Req->Success = 1;
        Req->Result = UHCI_OK;
        return UHCI_OK;
    }

    while (td) {
        cs = td->ControlStatus;
        if (cs & UHCI_TD_ACTIVE) {
            return UHCI_OK;
        }
        actual += (cs & UHCI_TD_ACTLEN_MASK);
        if (td->LinkPointer & UHCI_FRAME_TERMINATE) break;
        td = (UHCITransferDescriptor*)(u64)(td->LinkPointer & UHCI_FRAME_PTR_MASK);
    }

    Req->Completed = 1;
    Req->Success = 1;
    Req->ActualLength = actual;
    Req->Result = UHCI_OK;
    return UHCI_OK;
}

void UHCIClearFrameListEntry(UHCIContext* ctx, UHCIQueueHead* qh) {
    if (!ctx || !ctx->FrameList || !qh) return;
    u32 qh_phys = (u32)(u64)qh & ~0xF;
    for (int i = 0; i < UHCI_FRAME_LIST_SIZE; i++) {
        if ((ctx->FrameList[i].Pointer & ~0xF) == qh_phys) {
            ctx->FrameList[i].Pointer = UHCI_FRAME_TERMINATE;
        }
    }
}

UHCIResult UHCIPoll(UHCIContext* ctx)
{
    if (!ctx) return UHCI_ERROR_GENERAL;
    if (ctx->IsPolling) return UHCI_OK;

    ctx->IsPolling = 1;

    UHCIRequest* Cur = ctx->PendingRequests;
    UHCIRequest* Prev = NULL;
    UHCIRequest* CompletedHead = NULL;
    UHCIRequest* CompletedTail = NULL;

    while (Cur) {
        UHCIRequest* Next = Cur->Next;
        u8 should_remove = 0;

        if (Cur->QH && Cur->TD) {
            UHCIResult Result = UHCICheckRequestStatus(Cur);

            if (Cur->Completed) {
                should_remove = 1;
            } else if (Result != UHCI_OK) {
                Cur->ErrorCount++;
                if (Cur->ErrorCount >= 3) {
                    Cur->Completed = 1;
                    Cur->Result = Result;
                    should_remove = 1;
                }
            }
        } else {
            Cur->Completed = 1;
            Cur->Result = UHCI_ERROR_GENERAL;
            should_remove = 1;
        }

        if (should_remove) {
            if (Prev) {
                Prev->Next = Next;
            } else {
                ctx->PendingRequests = Next;
            }
            Cur->Next = NULL;

            if (CompletedTail) {
                CompletedTail->Next = Cur;
                CompletedTail = Cur;
            } else {
                CompletedHead = CompletedTail = Cur;
            }

            if (Cur->QH && ctx->FrameList) {
                ctx->FrameList[Cur->FrameIndex].Pointer = UHCI_FRAME_TERMINATE;
            }
        } else {
            Prev = Cur;
        }

        Cur = Next;
    }

    if (CompletedHead) {
        if (ctx->CompletedRequestsTail) {
            ctx->CompletedRequestsTail->Next = CompletedHead;
            ctx->CompletedRequestsTail = CompletedTail;
        } else {
            ctx->CompletedRequests = CompletedHead;
            ctx->CompletedRequestsTail = CompletedTail;
        }

        Cur = CompletedHead;
        while (Cur) {
            UHCIRequest* Next = Cur->Next;
            if (Cur->Callback) {
                Cur->Callback(Cur);
            }
            Cur = Next;
        }
    }

    u16 io = ctx->HC->IOBase;
    u16 sts = inw(io + UHCI_STS_OFFSET);
    if (sts) outw(io + UHCI_STS_OFFSET, sts);

    ctx->IsPolling = 0;
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