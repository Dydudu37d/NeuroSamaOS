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
UHCIResult WaitForRequestComplete(UHCIRequest *req, u32 timeout_us) {
    u32 elapsed = 0;
    u32 pollCount = 0;
    if (!req || !req->UHCI) return UHCI_ERROR_GENERAL;
    while (!req->Completed) {
        if (pollCount % 100 == 0) UHCIPoll(req->UHCI);
        for (u32 i = 0; i < 10; i++) __asm__ volatile ("pause");
        elapsed += 10;
        pollCount++;
        if (elapsed > timeout_us) return UHCI_ERROR_TIMEOUT;
    }
    return req->Result;
}
UHCIResult GetDeviceDescriptor(UHCIContext *ctx, u8 devAddr, USBDeviceDescriptor *desc, u8 is_low_speed) {
    u8 setup[8] __attribute__((aligned(16)));
    u8 data8[8] __attribute__((aligned(16)));
    UHCIResult result;
    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0100, 0x0000, 0x0008);
    result = ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, data8, 8, 1, is_low_speed, 8);
    DebugStr("GetDeviceDescriptor step result: ");
    DebugI64(result);
    DebugStr("\n");
    if (result != UHCI_OK) return result;
    u8 maxPacketSize0 = data8[7];
    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0100, 0x0000, 0x0012);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, (u8*)desc, 18, 1, is_low_speed, maxPacketSize0);
}
UHCIResult SetDeviceAddress(UHCIContext *ctx, u8 devAddr, u8 is_low_speed, u8 maxPacketSize) {
    u8 setup[8] __attribute__((aligned(16)));
    BuildSetupPacket(setup, 0x00, 0x05, devAddr, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, 0, 0, setup, 8, NULL, 0, 0, is_low_speed, maxPacketSize);
}
UHCIResult SetConfiguration(UHCIContext *ctx, u8 devAddr, u8 configValue, u8 is_low_speed, u8 maxPacketSize) {
    u8 setup[8] __attribute__((aligned(16)));
    BuildSetupPacket(setup, 0x00, USB_REQ_SET_CONFIG, configValue, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0, is_low_speed, maxPacketSize);
}
UHCIResult SetProtocol(UHCIContext *ctx, u8 devAddr, u8 protocol, u8 is_low_speed, u8 maxPacketSize) {
    u8 setup[8] __attribute__((aligned(16)));
    BuildSetupPacket(setup, 0x21, USB_REQ_SET_PROTOCOL, protocol, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0, is_low_speed, maxPacketSize);
}
UHCIResult SetIdle(UHCIContext *ctx, u8 devAddr, u8 duration, u8 is_low_speed, u8 maxPacketSize) {
    u8 setup[8] __attribute__((aligned(16)));
    BuildSetupPacket(setup, 0x21, USB_REQ_SET_IDLE, (duration << 8), 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0, is_low_speed, maxPacketSize);
}
UHCIResult CreateInterruptIN(UHCIContext *ctx, u8 devAddr, u8 endpoint, u8 *buffer, u16 len, u8 is_low_speed, UHCITransferDescriptor **outTD, u32 *outTDPhys) {
    AllocPool *pool = ctx->MemoryPool;
    UHCITransferDescriptor *td;
    u32 tdPhys;
    u32 ctrl_ls = is_low_speed ? UHCI_TD_LS : 0;
    td = (UHCITransferDescriptor*)MaxAlignedAlloc(pool, sizeof(UHCITransferDescriptor), 16, UHCI_MAX_ADDRESS);
    if (!td) return UHCI_ERROR_NO_MEMORY;
    tdPhys = (u32)(u64)td;
    MemSet(td, 0, sizeof(UHCITransferDescriptor));
    td->Token = UHCI_TD_PID_IN;
    td->Token |= ((devAddr & 0x7F) << 8);
    td->Token |= ((endpoint & 0xF) << 15);
    td->Token |= (0 << 19);
    td->Token |= (((len - 1) & 0x7FF) << 21);
    td->ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((len - 1) & 0x7FF);
    td->BufferPointer = (u32)(u64)buffer;
    td->LinkPointer = UHCI_FRAME_TERMINATE;
    *outTD = td;
    *outTDPhys = tdPhys;
    return UHCI_OK;
}
UHCIResult ExecuteControlTransfer(UHCIContext *ctx, u8 devAddr, u8 endpoint, u8 *setupPacket, u16 setupLen, u8 *data, u16 dataLen, u8 dir, u8 is_low_speed, u16 maxPacketSize){
    if (!ctx || !setupPacket || setupLen > 16) {
        return UHCI_ERROR_GENERAL;
    }
    if (maxPacketSize == 0) maxPacketSize = 8;

    int dataTdCount = 0;
    if (dataLen > 0 && data) {
        dataTdCount = (dataLen + maxPacketSize - 1) / maxPacketSize;
    }

    int totalTds = 1 + dataTdCount + 1;
    u32 qSize = 16 + totalTds * sizeof(UHCITransferDescriptor) + 64 + dataLen;

    void *q = MaxAlignedAlloc(ctx->MemoryPool, qSize, 16, 0xFFFFFFFFULL);
    if (!q) {
        return UHCI_ERROR_NO_MEMORY;
    }

    MemSet(q, 0, qSize);

    UHCIQueueHead *qh = (UHCIQueueHead*)q;
    UHCITransferDescriptor *tds = (UHCITransferDescriptor*)((u8*)q + 16);
    u8 *setupBuffer = (u8*)((u8*)tds + totalTds * sizeof(UHCITransferDescriptor));
    u8 *dataBuffer = (u8*)(setupBuffer + 64);

    u32 qhPhys = (u32)(u64)qh;
    u32 tdPhys = qhPhys + 16;

    MemCopy(setupBuffer, setupPacket, setupLen);
    if (dataLen > 0 && data && dir == 0) {
        MemCopy(dataBuffer, data, dataLen);
    }

    u32 ctrl_ls = is_low_speed ? UHCI_TD_LS : 0;

    tds[0].ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((setupLen - 1) & 0x7FF);
    tds[0].Token = UHCI_TD_PID_SETUP | ((devAddr & 0x7F) << 8) | ((endpoint & 0xF) << 15) | (((setupLen - 1) & 0x7FF) << 21);
    tds[0].BufferPointer = (u32)(u64)setupBuffer;

    if (dataLen > 0 && data) {
        u16 remaining = dataLen;
        u16 offset = 0;
        u8 toggle = 1;
        for (int i = 0; i < dataTdCount; i++) {
            u16 chunkSize = (remaining > maxPacketSize) ? maxPacketSize : remaining;
            int tdIdx = 1 + i;

            tds[tdIdx].ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | UHCI_TD_SPD | ((chunkSize - 1) & 0x7FF);
            tds[tdIdx].Token = (dir == 0 ? UHCI_TD_PID_OUT : UHCI_TD_PID_IN)
                | ((devAddr & 0x7F) << 8)
                | ((endpoint & 0xF) << 15)
                | (((chunkSize - 1) & 0x7FF) << 21)
                | (toggle << 19);
            tds[tdIdx].BufferPointer = (u32)(u64)(dataBuffer + offset);

            remaining -= chunkSize;
            offset += chunkSize;
            toggle ^= 1;
        }
    }

    int statusIdx = totalTds - 1;
    tds[statusIdx].ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | ctrl_ls | 0x7FF;
    tds[statusIdx].Token = UHCI_TD_PID_IN | ((devAddr & 0x7F) << 8) | ((endpoint & 0xF) << 15) | (1 << 19) | (0x7FF << 21);
    tds[statusIdx].BufferPointer = 0;

    if (dataTdCount > 0) {
        tds[0].LinkPointer = (tdPhys + sizeof(UHCITransferDescriptor)) & ~0xF;
        for (int i = 1; i < dataTdCount; i++) {
            tds[i].LinkPointer = (tdPhys + (i + 1) * sizeof(UHCITransferDescriptor)) & ~0xF;
        }
        tds[dataTdCount].LinkPointer = (tdPhys + statusIdx * sizeof(UHCITransferDescriptor)) & ~0xF;
    } else {
        tds[0].LinkPointer = (tdPhys + statusIdx * sizeof(UHCITransferDescriptor)) & ~0xF;
    }
    tds[statusIdx].LinkPointer = UHCI_FRAME_TERMINATE;

    qh->HorizontalLink = UHCI_FRAME_TERMINATE;
    qh->VerticalLink = (tdPhys & ~0xF) | UHCI_QH_VERTICAL;

    u16 frameIndex = 0;
    u32 oldFrameHead = ctx->FrameList[frameIndex].Pointer;

    UHCIStop(ctx);
    ctx->FrameList[frameIndex].Pointer = (qhPhys & ~0xF) | UHCI_FRAME_QH;
    __asm__ volatile("mfence" ::: "memory");
    UHCIStart(ctx);

    u32 timeout = 5000000;
    UHCIResult result = UHCI_ERROR_TIMEOUT;

    while (timeout--) {
        UHCIPoll(ctx);
        u16 sts = inw(ctx->HC->IOBase + UHCI_STS_OFFSET);
        if (sts) outw(ctx->HC->IOBase + UHCI_STS_OFFSET, sts);
        __asm__ volatile("mfence" ::: "memory");

        u32 final_cs = tds[statusIdx].ControlStatus;
        if (!(final_cs & UHCI_TD_ACTIVE)) {
            if (final_cs & UHCI_TD_STALL) {
                result = UHCI_ERROR_STALL;
            } else {
                if (tds[0].ControlStatus & UHCI_TD_STALL) {
                    result = UHCI_ERROR_STALL;
                } else {
                    for (int i = 0; i < dataTdCount; i++) {
                        if (tds[1+i].ControlStatus & UHCI_TD_STALL) {
                            result = UHCI_ERROR_STALL;
                            break;
                        }
                    }
                    if (result != UHCI_ERROR_STALL) {
                        if (dataLen > 0 && data && dir == 1) {
                            MemCopy(data, dataBuffer, dataLen);
                        }
                        result = UHCI_OK;
                    }
                }
            }
            break;
        }
        if (tds[0].ControlStatus & UHCI_TD_STALL) {
            result = UHCI_ERROR_STALL;
            break;
        }
        inb(0x80);
        __asm__ volatile("pause");
    }

    UHCIStop(ctx);
    ctx->FrameList[frameIndex].Pointer = oldFrameHead;
    UHCIStart(ctx);
    SystemBusySleepMs(1);
    Free(ctx->MemoryPool, q);
    return result;
}
UHCIHostController* UHCICreate(u8 Bus, u8 Slot, u8 Func, AllocPool* Pool) {
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
    hc->IOBase = 0;
    if ((bar4 & 1) && (bar4 & 0xFFFE) != 0) hc->IOBase = bar4 & 0xFFFE;
    else if ((bar0 & 1) && (bar0 & 0xFFFE) != 0) hc->IOBase = bar0 & 0xFFFE;
    else return NULL;
    return hc;
}
UHCIResult UHCIInitialize(UHCIContext* ctx) {
    if (!ctx || !ctx->HC || ctx->HC->IOBase == 0) return UHCI_ERROR_GENERAL;
    ctx->FrameListSize = UHCI_FRAME_LIST_SIZE;
    ctx->MaxRequests = UHCI_MAX_REQUESTS;
    ctx->Initialized = 0;
    ctx->PollMode = 1;
    ctx->IsPolling = 0;
    ctx->CompletedRequests = NULL;
    ctx->CompletedRequestsTail = NULL;
    ctx->PendingRequests = NULL;
    ctx->FrameList = (UHCIFrameListEntry*)MaxAlignedAlloc(ctx->HC->MemoryPool, UHCI_FRAME_LIST_BYTES, 4096, UHCI_MAX_ADDRESS);
    if (!ctx->FrameList) return UHCI_ERROR_NO_MEMORY;
    MemSet(ctx->FrameList, 0, UHCI_FRAME_LIST_BYTES);
    ctx->FrameListPhys = (u32)(u64)ctx->FrameList;
    ctx->FrameListVirt = (void*)ctx->FrameList;
    for (u32 i = 0; i < UHCI_FRAME_LIST_SIZE; i++) ctx->FrameList[i].Pointer = UHCI_FRAME_TERMINATE;
    UHCIResult res = UHCIReset(ctx);
    if (res != UHCI_OK) return res;
    outl(ctx->HC->IOBase + UHCI_FRBASE_OFFSET, ctx->FrameListPhys);
    outw(ctx->HC->IOBase + UHCI_FRNUM_OFFSET, 0);
    outw(ctx->HC->IOBase + UHCI_INTR_OFFSET, 0);
    ctx->Initialized = 1;
    return UHCI_OK;
}
UHCIResult UHCIReset(UHCIContext* ctx) {
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    u16 io = ctx->HC->IOBase;
    outw(io + UHCI_CMD_OFFSET, inw(io + UHCI_CMD_OFFSET) & ~UHCI_CMD_RS);
    SystemBusySleepMs(10);
    outw(io + UHCI_CMD_OFFSET, UHCI_CMD_HCRESET);
    int timeout = 10000;
    while (inw(io + UHCI_CMD_OFFSET) & UHCI_CMD_HCRESET) {
        if (--timeout <= 0) return UHCI_ERROR_TIMEOUT;
        SystemBusySleepMs(1);
    }
    timeout = 10000;
    while (!(inw(io + UHCI_STS_OFFSET) & UHCI_STS_HCHALTED)) {
        if (--timeout <= 0) return UHCI_ERROR_TIMEOUT;
        SystemBusySleepMs(1);
    }
    return UHCI_OK;
}
UHCIResult UHCIConfigure(UHCIContext* ctx, u8 maxPacket, u8 debugMode) {
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    ctx->HC->MaxPacket = maxPacket;
    ctx->HC->DebugMode = debugMode;
    u16 io = ctx->HC->IOBase;
    u16 cmd = inw(io + UHCI_CMD_OFFSET);
    if (maxPacket == 64) cmd |= UHCI_CMD_MAXP; else cmd &= ~UHCI_CMD_MAXP;
    if (debugMode) cmd |= UHCI_CMD_SWDBG; else cmd &= ~UHCI_CMD_SWDBG;
    outw(io + UHCI_CMD_OFFSET, cmd);
    ctx->HC->Configured = 1;
    return UHCI_OK;
}
UHCIResult UHCIStart(UHCIContext* ctx) {
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
    for (int i = 0; i < 16; i++) ctx->FrameList[i].Pointer = ctx->FrameList[i].Pointer;
    __asm__ volatile("mfence" ::: "memory");
    UHCIDumpFrameList(ctx, 0, 8);
    SystemBusySleepMs(50);
    return UHCI_OK;
}
UHCIResult UHCIStop(UHCIContext* ctx) {
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    u16 io = ctx->HC->IOBase;
    outw(io + UHCI_CMD_OFFSET, inw(io + UHCI_CMD_OFFSET) & ~UHCI_CMD_RS);
    ctx->HC->Running = 0;
    return UHCI_OK;
}
u8 UHCIIsHalted(UHCIContext* ctx) {
    if (!ctx || !ctx->HC) return 1;
    return (inw(ctx->HC->IOBase + UHCI_STS_OFFSET) & UHCI_STS_HCHALTED) ? 1 : 0;
}
u8 UHCIIsRunning(UHCIContext* ctx) {
    if (!ctx || !ctx->HC) return 0;
    return (inw(ctx->HC->IOBase + UHCI_CMD_OFFSET) & UHCI_CMD_RS) ? 1 : 0;
}
u16 UHCIGetFrameNumber(UHCIContext* ctx) {
    if (!ctx || !ctx->HC) return 0;
    return inw(ctx->HC->IOBase + UHCI_FRNUM_OFFSET);
}
UHCIResult UHCISetFrameNumber(UHCIContext* ctx, u16 Frame) {
    if (!ctx || !ctx->HC) return UHCI_ERROR_GENERAL;
    outw(ctx->HC->IOBase + UHCI_FRNUM_OFFSET, Frame & 0x3FF);
    return UHCI_OK;
}
u16 UHCIGetPortStatus(UHCIContext* ctx, u8 Port) {
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return 0;
    return inw(ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2));
}
UHCIResult UHCIResetPort(UHCIContext* ctx, u8 Port) {
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 portRegister = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    UHCISafeWritePort(portRegister, UHCI_PORTSC_PR, 0);
    SystemBusySleepMs(UHCI_PORT_RESET_DELAY_MS);
    UHCISafeWritePort(portRegister, 0, UHCI_PORTSC_PR);
    SystemBusySleepMs(150);
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
UHCIResult UHCIEnablePort(UHCIContext* ctx, u8 Port) {
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    UHCISafeWritePort(reg, UHCI_PORTSC_PE, 0);
    return UHCI_OK;
}
UHCIResult UHCIDisablePort(UHCIContext* ctx, u8 Port) {
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    UHCISafeWritePort(reg, 0, UHCI_PORTSC_PE);
    return UHCI_OK;
}
UHCIResult UHCISuspendPort(UHCIContext* ctx, u8 Port) {
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    UHCISafeWritePort(reg, UHCI_PORTSC_SUSP, 0);
    return UHCI_OK;
}
UHCIResult UHCIResumePort(UHCIContext* ctx, u8 Port) {
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    UHCISafeWritePort(reg, UHCI_PORTSC_RD, 0);
    SystemBusySleepMs(UHCI_RESUME_DELAY_MS);
    UHCISafeWritePort(reg, 0, UHCI_PORTSC_RD);
    return UHCI_OK;
}
UHCIResult UHCIClearPortChange(UHCIContext* ctx, u8 Port) {
    if (!ctx || !ctx->HC || Port >= UHCI_PORT_COUNT) return UHCI_ERROR_GENERAL;
    u16 reg = ctx->HC->IOBase + UHCI_PORTSC1_OFFSET + (Port * 2);
    u16 val = inw(reg);
    outw(reg, (val & ~UHCI_PORTSC_W1C) | UHCI_PORTSC_W1C);
    return UHCI_OK;
}
UHCIRequest* UHCICreateRequest(UHCIContext* ctx) {
    if (!ctx || !ctx->HC) return NULL;
    UHCIRequest* req = (UHCIRequest*)UHCIMemAlloc(ctx->HC->MemoryPool, sizeof(UHCIRequest));
    if (!req) return NULL;
    MemSet(req, 0, sizeof(UHCIRequest));
    req->UHCI = ctx;
    return req;
}
UHCIResult UHCISubmitRequest(UHCIContext* ctx, UHCIRequest* Req) {
    if (!ctx || !Req || !ctx->HC) return UHCI_ERROR_GENERAL;
    Req->Completed = 0;
    Req->Active = 1;
    Req->Success = 0;
    Req->Result = UHCI_OK;
    Req->Next = NULL;
    Req->ErrorCount = 0;
    if (!ctx->PendingRequests) ctx->PendingRequests = Req;
    else {
        UHCIRequest* last = ctx->PendingRequests;
        while (last->Next) last = last->Next;
        last->Next = Req;
    }
    return UHCI_OK;
}
UHCIResult UHCICancelRequest(UHCIContext* ctx, UHCIRequest* req) {
    if (!ctx || !req) return UHCI_ERROR_GENERAL;
    UHCIRequest* Cur = ctx->PendingRequests;
    UHCIRequest* Prev = NULL;
    while (Cur) {
        if (Cur == req) {
            if (req->QH && ctx->FrameList) ctx->FrameList[req->FrameIndex].Pointer = UHCI_FRAME_TERMINATE;
            if (Prev) Prev->Next = Cur->Next;
            else ctx->PendingRequests = Cur->Next;
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
static UHCIResult UHCICheckRequestStatus(UHCIRequest* Req) {
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
    if (cs & UHCI_TD_ACTIVE) return UHCI_OK;
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
    Req->Completed = 1;
    Req->Success = 1;
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
void BuildSetupPacket(u8 *packet, u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength) {
    packet[0] = bmRequestType;
    packet[1] = bRequest;
    packet[2] = wValue & 0xFF;
    packet[3] = (wValue >> 8) & 0xFF;
    packet[4] = wIndex & 0xFF;
    packet[5] = (wIndex >> 8) & 0xFF;
    packet[6] = wLength & 0xFF;
    packet[7] = (wLength >> 8) & 0xFF;
}
UHCIResult USBSetAddress(UHCIContext *ctx, u8 newAddr, u8 is_low_speed) {
    u8 setup[8] __attribute__((aligned(16)));
    BuildSetupPacket(setup, 0x00, 0x05, newAddr, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, 0, 0, setup, 8, NULL, 0, 0, is_low_speed, 8);
}
UHCIResult UHCIPoll(UHCIContext* ctx) {
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
            if (Cur->Completed) should_remove = 1;
            else if (Result != UHCI_OK) {
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
            if (Prev) Prev->Next = Next;
            else ctx->PendingRequests = Next;
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
            if (Cur->Callback) Cur->Callback(Cur);
            Cur = Next;
        }
    }
    u16 io = ctx->HC->IOBase;
    u16 sts = inw(io + UHCI_STS_OFFSET);
    if (sts) outw(io + UHCI_STS_OFFSET, sts);
    ctx->IsPolling = 0;
    return UHCI_OK;
}
void UHCIDumpStatus(UHCIContext* ctx) {
    if (!ctx || !ctx->HC) return;
    u16 io = ctx->HC->IOBase;
    u16 cmd = inw(io + UHCI_CMD_OFFSET);
    u16 sts = inw(io + UHCI_STS_OFFSET);
    u16 intr = inw(io + UHCI_INTR_OFFSET);
    u16 frnum = inw(io + UHCI_FRNUM_OFFSET);
    u32 frbase = inl(io + UHCI_FRBASE_OFFSET);
    DebugStr("UHCI IOBase: 0x"); DebugU16(io); DebugChar('\n');
    DebugStr("CMD: 0x"); DebugU16(cmd); DebugStr(", STS: 0x"); DebugU16(sts); DebugChar('\n');
    DebugStr("INTR: 0x"); DebugU16(intr); DebugStr(", FRNUM: 0x"); DebugU16(frnum); DebugChar('\n');
    DebugStr("FRBASE: 0x"); DebugU32(frbase); DebugChar('\n');
    for(u8 i = 0; i < ctx->HC->PortCount; i++) {
        u16 psc = inw(io + UHCI_PORTSC1_OFFSET + (i * 2));
        DebugStr("PORTSC"); DebugU8(i + 1); DebugStr(": 0x"); DebugU16(psc); DebugChar('\n');
    }
}
void UHCIDumpFrameList(UHCIContext* ctx, u16 start, u16 count) {
    if (!ctx || !ctx->FrameList || start >= UHCI_FRAME_LIST_SIZE) return;
    u16 end = start + count;
    if (end > UHCI_FRAME_LIST_SIZE) end = UHCI_FRAME_LIST_SIZE;
    DebugStr("--- FrameList Dump ---\n");
    for(u16 i = start; i < end; i++) {
        u32 val = ctx->FrameList[i].Pointer;
        if (val != UHCI_FRAME_TERMINATE) {
            DebugStr("Frame ["); DebugU16(i); DebugStr("]: 0x"); DebugU32(val); DebugChar('\n');
        }
    }
}