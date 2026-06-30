#include "uhci.h"
#include "debug.h"
#include "clock.h"
#include "str.h"
#include "flash.h"

static inline void UHCIOutW(UHCIHostController *HC, u16 Offset, u16 Value)
{
    outw(Value, HC->IOBase + Offset);
}

static inline u16 UHCIInW(UHCIHostController *HC, u16 Offset)
{
    return inw(HC->IOBase + Offset);
}

static inline void UHCIOutL(UHCIHostController *HC, u16 Offset, u32 Value)
{
    outl(Value, HC->IOBase + Offset);
}

static inline u32 UHCIInL(UHCIHostController *HC, u16 Offset)
{
    return inl(HC->IOBase + Offset);
}

static inline void UHCIOutB(UHCIHostController *HC, u16 Offset, u8 Value)
{
    outb(Value, HC->IOBase + Offset);
}

static inline u8 UHCIInB(UHCIHostController *HC, u16 Offset)
{
    return inb(HC->IOBase + Offset);
}

static inline void UHCISetCommandBit(UHCIHostController *HC, u16 Bit, u8 Set)
{
    u16 Cmd = UHCIInW(HC, UHCI_CMD_OFFSET);
    if (Set)
        Cmd |= Bit;
    else
        Cmd &= ~Bit;
    UHCIOutW(HC, UHCI_CMD_OFFSET, Cmd);
}

static inline u8 UHCIGetStatusBit(UHCIHostController *HC, u16 Bit)
{
    return (UHCIInW(HC, UHCI_STS_OFFSET) & Bit) != 0;
}

static inline void UHCIClearStatusBit(UHCIHostController *HC, u16 Bit)
{
    UHCIOutW(HC, UHCI_STS_OFFSET, Bit);
}

static inline void UHCIWait(u32 Microseconds)
{
    u64 Start = rdtsc_serialized();
    u64 TicksPerUs = GetTscFrequency() / 1000000;
    u64 Target = Start + (Microseconds * TicksPerUs);
    while (rdtsc_serialized() < Target)
    {
        __asm__ volatile("pause");
    }
}

static inline UHCIResult UHCIWaitForBits(UHCIHostController *HC, u16 Offset, u16 Mask, u16 Value, u32 TimeoutMs)
{
    u64 Start = SystemGetTimeMillis();
    while (1)
    {
        u16 Reg = UHCIInW(HC, Offset);
        if ((Reg & Mask) == Value)
            return UHCI_OK;
        if (SystemGetTimeMillis() - Start > TimeoutMs)
            return UHCI_ERROR_TIMEOUT;
        UHCIWait(10);
    }
}

UHCIHostController *UHCICreate(u8 Bus, u8 Slot, u8 Func, AllocPool *Pool)
{
    UHCIHostController *HC = Alloc(Pool, sizeof(UHCIHostController));
    if (!HC)
        return 0;

    HC->Bus = Bus;
    HC->Slot = Slot;
    HC->Func = Func;
    HC->IOBase = (u16)PCIGetBARAddress(Bus, Slot, Func, 0);
    HC->MemoryPool = Pool;
    HC->Running = 0;
    HC->Configured = 0;
    HC->MaxPacket = UHCI_RECLAIM_64;
    HC->DebugMode = 0;
    HC->FrameListPhys = 0;
    HC->FrameListVirt = 0;
    HC->Suspended = 0;
    HC->GlobalSuspend = 0;
    HC->ResumeSignaling = 0;

    PCIEnableDevice(Bus, Slot, Func);

    return HC;
}

UHCIResult UHCIInitialize(UHCIContext *Ctx)
{
    if (!Ctx || !Ctx->HC)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;

    UHCIOutW(HC, UHCI_CMD_OFFSET, UHCI_CMD_HCRESET);
    UHCIWait(50);

    UHCIResult Result = UHCIWaitForBits(HC, UHCI_CMD_OFFSET, UHCI_CMD_HCRESET, 0, 100);
    if (Result != UHCI_OK)
        return Result;

    UHCIOutW(HC, UHCI_INTR_OFFSET, 0);

    if (!Ctx->FrameList)
    {
        Ctx->FrameList = Alloc(Ctx->MemoryPool, UHCI_FRAME_LIST_BYTES);
        if (!Ctx->FrameList)
            return UHCI_ERROR_NO_MEMORY;
        MemSet(Ctx->FrameList, 0, UHCI_FRAME_LIST_BYTES);
    }

    HC->FrameListVirt = Ctx->FrameList;
    HC->FrameListPhys = (u32)(u64)Ctx->FrameList;

    UHCIOutL(HC, UHCI_FRBASE_OFFSET, HC->FrameListPhys);
    UHCIOutW(HC, UHCI_FRNUM_OFFSET, 0);
    UHCIOutB(HC, UHCI_SOFMOD_OFFSET, 0x40);

    for (u8 Port = 0; Port < UHCI_PORT_COUNT; Port++)
    {
        UHCIOutW(HC, UHCI_PORTSC1_OFFSET + (Port * 2), 0);
        UHCIWait(20);
    }

    Ctx->Initialized = 1;
    Ctx->Suspended = 0;
    Ctx->GlobalSuspend = 0;
    Ctx->ResumeSignaling = 0;
    Ctx->PendingRequests = 0;
    Ctx->CompletedRequests = 0;
    Ctx->FreeRequests = 0;

    return UHCI_OK;
}

UHCIResult UHCIStart(UHCIContext *Ctx)
{
    if (!Ctx || !Ctx->Initialized)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;

    UHCISetCommandBit(HC, UHCI_CMD_CF, 1);
    UHCISetCommandBit(HC, UHCI_CMD_RS, 1);

    HC->Running = 1;

    return UHCI_OK;
}

UHCIResult UHCIStop(UHCIContext *Ctx)
{
    if (!Ctx || !Ctx->HC)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;

    UHCISetCommandBit(HC, UHCI_CMD_RS, 0);

    UHCIResult Result = UHCIWaitForBits(HC, UHCI_STS_OFFSET, UHCI_STS_HCHALTED, UHCI_STS_HCHALTED, 100);
    if (Result != UHCI_OK)
        return Result;

    HC->Running = 0;

    return UHCI_OK;
}

UHCIResult UHCIReset(UHCIContext *Ctx)
{
    if (!Ctx || !Ctx->HC)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;

    if (HC->Running)
    {
        UHCISetCommandBit(HC, UHCI_CMD_RS, 0);
        UHCIWaitForBits(HC, UHCI_STS_OFFSET, UHCI_STS_HCHALTED, UHCI_STS_HCHALTED, 100);
        HC->Running = 0;
    }

    UHCIOutW(HC, UHCI_CMD_OFFSET, UHCI_CMD_GRESET);
    UHCIWait(50);

    UHCIResult Result = UHCIWaitForBits(HC, UHCI_CMD_OFFSET, UHCI_CMD_GRESET, 0, 100);
    if (Result != UHCI_OK)
        return Result;

    UHCIOutW(HC, UHCI_INTR_OFFSET, 0);
    UHCIOutL(HC, UHCI_FRBASE_OFFSET, HC->FrameListPhys);
    UHCIOutW(HC, UHCI_FRNUM_OFFSET, 0);
    UHCIOutB(HC, UHCI_SOFMOD_OFFSET, 0x40);

    for (u8 Port = 0; Port < UHCI_PORT_COUNT; Port++)
    {
        UHCIOutW(HC, UHCI_PORTSC1_OFFSET + (Port * 2), 0);
        UHCIWait(20);
    }

    Ctx->Initialized = 1;
    Ctx->PendingRequests = 0;
    Ctx->CompletedRequests = 0;

    return UHCI_OK;
}

UHCIResult UHCIConfigure(UHCIContext *Ctx, u8 MaxPacket, u8 DebugMode)
{
    if (!Ctx || !Ctx->HC)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;

    if (MaxPacket)
    {
        UHCISetCommandBit(HC, UHCI_CMD_MAXP, 1);
        HC->MaxPacket = UHCI_RECLAIM_64;
    }
    else
    {
        UHCISetCommandBit(HC, UHCI_CMD_MAXP, 0);
        HC->MaxPacket = UHCI_RECLAIM_32;
    }

    if (DebugMode)
    {
        UHCISetCommandBit(HC, UHCI_CMD_SWDBG, 1);
        HC->DebugMode = 1;
    }
    else
    {
        UHCISetCommandBit(HC, UHCI_CMD_SWDBG, 0);
        HC->DebugMode = 0;
    }

    HC->Configured = 1;

    return UHCI_OK;
}

UHCIResult UHCIResetPort(UHCIContext *Ctx, u8 Port)
{
    if (!Ctx || !Ctx->HC || Port >= UHCI_PORT_COUNT)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;
    u16 Offset = UHCI_PORTSC1_OFFSET + (Port * 2);

    u16 PortSc = UHCIInW(HC, Offset);
    PortSc |= UHCI_PORTSC_PR;
    UHCIOutW(HC, Offset, PortSc);

    UHCIWait(50000);

    PortSc = UHCIInW(HC, Offset);
    PortSc &= ~UHCI_PORTSC_PR;
    UHCIOutW(HC, Offset, PortSc);

    UHCIWait(10000);

    return UHCI_OK;
}

UHCIResult UHCIEnablePort(UHCIContext *Ctx, u8 Port)
{
    if (!Ctx || !Ctx->HC || Port >= UHCI_PORT_COUNT)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;
    u16 Offset = UHCI_PORTSC1_OFFSET + (Port * 2);

    u16 PortSc = UHCIInW(HC, Offset);
    PortSc |= UHCI_PORTSC_PE;
    UHCIOutW(HC, Offset, PortSc);

    return UHCI_OK;
}

UHCIResult UHCIDisablePort(UHCIContext *Ctx, u8 Port)
{
    if (!Ctx || !Ctx->HC || Port >= UHCI_PORT_COUNT)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;
    u16 Offset = UHCI_PORTSC1_OFFSET + (Port * 2);

    u16 PortSc = UHCIInW(HC, Offset);
    PortSc &= ~UHCI_PORTSC_PE;
    UHCIOutW(HC, Offset, PortSc);

    return UHCI_OK;
}

UHCIResult UHCISuspendPort(UHCIContext *Ctx, u8 Port)
{
    if (!Ctx || !Ctx->HC || Port >= UHCI_PORT_COUNT)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;
    u16 Offset = UHCI_PORTSC1_OFFSET + (Port * 2);

    u16 PortSc = UHCIInW(HC, Offset);
    PortSc |= UHCI_PORTSC_SUSP;
    UHCIOutW(HC, Offset, PortSc);

    return UHCI_OK;
}

UHCIResult UHCIResumePort(UHCIContext *Ctx, u8 Port)
{
    if (!Ctx || !Ctx->HC || Port >= UHCI_PORT_COUNT)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;
    u16 Offset = UHCI_PORTSC1_OFFSET + (Port * 2);

    u16 PortSc = UHCIInW(HC, Offset);
    PortSc |= UHCI_PORTSC_RD;
    UHCIOutW(HC, Offset, PortSc);

    UHCIWait(20000);

    PortSc = UHCIInW(HC, Offset);
    PortSc &= ~UHCI_PORTSC_RD;
    PortSc &= ~UHCI_PORTSC_SUSP;
    UHCIOutW(HC, Offset, PortSc);

    return UHCI_OK;
}

u16 UHCIGetPortStatus(UHCIContext *Ctx, u8 Port)
{
    if (!Ctx || !Ctx->HC || Port >= UHCI_PORT_COUNT)
        return 0;

    UHCIHostController *HC = Ctx->HC;
    u16 Offset = UHCI_PORTSC1_OFFSET + (Port * 2);

    return UHCIInW(HC, Offset);
}

UHCIResult UHCIClearPortChange(UHCIContext *Ctx, u8 Port)
{
    if (!Ctx || !Ctx->HC || Port >= UHCI_PORT_COUNT)
        return UHCI_ERROR_GENERAL;

    UHCIHostController *HC = Ctx->HC;
    u16 Offset = UHCI_PORTSC1_OFFSET + (Port * 2);

    u16 PortSc = UHCIInW(HC, Offset);
    u16 ChangeBits = 0;

    if (PortSc & UHCI_PORTSC_CSC)
        ChangeBits |= UHCI_PORTSC_CSC;
    if (PortSc & UHCI_PORTSC_PEC)
        ChangeBits |= UHCI_PORTSC_PEC;

    if (ChangeBits)
    {
        UHCIOutW(HC, Offset, PortSc | ChangeBits);
    }

    return UHCI_OK;
}

UHCIRequest *UHCICreateRequest(UHCIContext *Ctx)
{
    if (!Ctx)
        return 0;

    UHCIRequest *Req = Alloc(Ctx->MemoryPool, sizeof(UHCIRequest));
    if (!Req)
        return 0;

    MemSet(Req, 0, sizeof(UHCIRequest));

    Req->TD = Alloc(Ctx->MemoryPool, sizeof(UHCITransferDescriptor));
    if (!Req->TD)
    {
        Free(Ctx->MemoryPool, Req);
        return 0;
    }
    MemSet(Req->TD, 0, sizeof(UHCITransferDescriptor));

    Req->QH = Alloc(Ctx->MemoryPool, sizeof(UHCIQueueHead));
    if (!Req->QH)
    {
        Free(Ctx->MemoryPool, Req->TD);
        Free(Ctx->MemoryPool, Req);
        return 0;
    }
    MemSet(Req->QH, 0, sizeof(UHCIQueueHead));

    return Req;
}

static void UHCISetupTD(UHCIRequest *Req)
{
    UHCITransferDescriptor *TD = Req->TD;

    TD->ControlStatus = UHCI_TD_ACTIVE;
    TD->ControlStatus |= ((Req->ErrorCount & 0x0F) << 27);

    if (Req->MaxPacketSize == 0)
        Req->MaxPacketSize = 8;
    TD->ControlStatus |= (Req->MaxPacketSize << 16);

    u8 Pid = 0;
    if (Req->Direction == UHCI_TRANSFER_SETUP)
    {
        Pid = UHCI_TD_PID_SETUP;
    }
    else if (Req->Direction == UHCI_TRANSFER_IN)
    {
        Pid = UHCI_TD_PID_IN;
    }
    else
    {
        Pid = UHCI_TD_PID_OUT;
    }

    TD->Token = (Req->DeviceAddress << 8) | (Req->Endpoint << 15) | (Pid << 24) | (Req->DataLength & 0x7FF);
    TD->BufferPointer = (u32)(u64)Req->DataBuffer;
    TD->LinkPointer = 0x00000001;
}

static UHCIResult UHCISubmitQueue(UHCIContext *Ctx, UHCIRequest *Req)
{
    u32 TdPhys = (u32)(u64)Req->TD;
    u32 QhPhys = (u32)(u64)Req->QH;

    Req->QH->HorizontalLink = 0x00000001;
    Req->QH->VerticalLink = TdPhys & ~0x0F;
    Req->QH->VerticalLink |= 0x02;

    UHCIFrameListEntry *Entry = &Ctx->FrameList[0];
    Entry->Pointer = QhPhys & ~0x0F;
    Entry->Pointer |= 0x02;

    return UHCI_OK;
}

UHCIResult UHCISubmitRequest(UHCIContext *Ctx, UHCIRequest *Req)
{
    if (!Ctx || !Req || !Ctx->HC)
        return UHCI_ERROR_GENERAL;

    Req->Active = 1;
    Req->Completed = 0;
    Req->Success = 0;
    Req->Stalled = 0;
    Req->Result = UHCI_OK;
    Req->ErrorCount = 0;

    UHCISetupTD(Req);

    UHCIResult Result = UHCISubmitQueue(Ctx, Req);
    if (Result != UHCI_OK)
        return Result;

    Req->Next = Ctx->PendingRequests;
    Ctx->PendingRequests = Req;

    return UHCI_OK;
}

UHCIResult UHCICancelRequest(UHCIContext *Ctx, UHCIRequest *Req)
{
    if (!Ctx || !Req)
        return UHCI_ERROR_GENERAL;

    Req->Active = 0;
    Req->Completed = 1;
    Req->Result = UHCI_ERROR_GENERAL;

    return UHCI_OK;
}

static UHCIResult UHCIPollTD(UHCIContext *Ctx, UHCIRequest *Req)
{
    UHCITransferDescriptor *TD = Req->TD;

    MemFullFlash();
    u32 ControlStatus = TD->ControlStatus;
    MemFullFlash();

    if (ControlStatus & UHCI_TD_ACTIVE)
    {
        return UHCI_OK;
    }

    Req->Active = 0;
    Req->Completed = 1;

    if (ControlStatus & UHCI_TD_STALL)
    {
        Req->Result = UHCI_ERROR_STALL;
        Req->Stalled = 1;
        return UHCI_ERROR_STALL;
    }

    if (ControlStatus & UHCI_TD_CRCERR)
    {
        Req->Result = UHCI_ERROR_CRC;
        return UHCI_ERROR_CRC;
    }

    if (ControlStatus & UHCI_TD_BITSTUFF)
    {
        Req->Result = UHCI_ERROR_BITSTUFF;
        return UHCI_ERROR_BITSTUFF;
    }

    if (ControlStatus & UHCI_TD_NAK)
    {
        Req->Result = UHCI_ERROR_NAK;
        return UHCI_ERROR_NAK;
    }

    Req->Success = 1;
    Req->Result = UHCI_OK;

    u16 ActualLen = (ControlStatus >> 16) & 0x7FF;
    Req->ActualLength = ActualLen;

    return UHCI_OK;
}

static void UHCIPollPendingRequests(UHCIContext *Ctx)
{
    UHCIRequest *Prev = 0;
    UHCIRequest *Cur = Ctx->PendingRequests;

    while (Cur)
    {
        UHCIRequest *Next = Cur->Next;

        UHCIResult Result = UHCIPollTD(Ctx, Cur);

        if (Result != UHCI_OK || Cur->Completed)
        {
            if (Prev)
            {
                Prev->Next = Next;
            }
            else
            {
                Ctx->PendingRequests = Next;
            }

            Cur->Next = Ctx->CompletedRequests;
            Ctx->CompletedRequests = Cur;

            if (Ctx->HandleCompletion && Cur->Success)
            {
                Ctx->HandleCompletion(Cur);
            }

            if (Ctx->HandleError && !Cur->Success && Cur->Result != UHCI_OK)
            {
                Ctx->HandleError(Cur, Cur->Result);
            }

            if (Cur->Callback)
            {
                Cur->Callback(Cur);
            }
        }
        else
        {
            Prev = Cur;
        }

        Cur = Next;
    }
}

static void UHCIPollPorts(UHCIContext *Ctx)
{
    for (u8 Port = 0; Port < UHCI_PORT_COUNT; Port++)
    {
        u16 Status = UHCIGetPortStatus(Ctx, Port);

        if (Status & (UHCI_PORTSC_CSC | UHCI_PORTSC_PEC))
        {
            UHCIClearPortChange(Ctx, Port);

            if (Ctx->HandlePortChange)
            {
                Ctx->HandlePortChange(Port, Status);
            }
        }
    }
}

UHCIResult UHCIPoll(UHCIContext *Ctx)
{
    if (!Ctx || !Ctx->HC)
        return UHCI_ERROR_GENERAL;

    if (!Ctx->HC->Running)
    {
        return UHCI_ERROR_GENERAL;
    }

    if (UHCIIsHalted(Ctx))
    {
        return UHCI_ERROR_PROCESS;
    }

    u16 Status = UHCIInW(Ctx->HC, UHCI_STS_OFFSET);
    if (Status & UHCI_STS_SYSERR)
    {
        UHCIClearStatusBit(Ctx->HC, UHCI_STS_SYSERR);
        return UHCI_ERROR_SYSTEM;
    }

    if (Status & UHCI_STS_PROCERR)
    {
        UHCIClearStatusBit(Ctx->HC, UHCI_STS_PROCERR);
        return UHCI_ERROR_PROCESS;
    }

    UHCIPollPendingRequests(Ctx);
    UHCIPollPorts(Ctx);

    return UHCI_OK;
}

u8 UHCIIsHalted(UHCIContext *Ctx)
{
    if (!Ctx || !Ctx->HC)
        return 1;
    return UHCIGetStatusBit(Ctx->HC, UHCI_STS_HCHALTED);
}

u8 UHCIIsRunning(UHCIContext *Ctx)
{
    if (!Ctx || !Ctx->HC)
        return 0;
    return Ctx->HC->Running;
}

u16 UHCIGetFrameNumber(UHCIContext *Ctx)
{
    if (!Ctx || !Ctx->HC)
        return 0;
    return UHCIInW(Ctx->HC, UHCI_FRNUM_OFFSET);
}

UHCIResult UHCISetFrameNumber(UHCIContext *Ctx, u16 Frame)
{
    if (!Ctx || !Ctx->HC)
        return UHCI_ERROR_GENERAL;

    if (Ctx->HC->Running)
    {
        return UHCI_ERROR_GENERAL;
    }

    UHCIOutW(Ctx->HC, UHCI_FRNUM_OFFSET, Frame);
    return UHCI_OK;
}

void UHCIDumpStatus(UHCIContext *Ctx)
{
    if (!Ctx || !Ctx->HC)
        return;

    DebugStr("UHCI Status:\r\n");
    DebugStr("  Running: ");
    DebugU64(Ctx->HC->Running);
    DebugStr("\r\n");
    DebugStr("  Configured: ");
    DebugU64(Ctx->HC->Configured);
    DebugStr("\r\n");
    DebugStr("  MaxPacket: ");
    DebugU64(Ctx->HC->MaxPacket);
    DebugStr("\r\n");
    DebugStr("  DebugMode: ");
    DebugU64(Ctx->HC->DebugMode);
    DebugStr("\r\n");

    u16 Cmd = UHCIInW(Ctx->HC, UHCI_CMD_OFFSET);
    u16 Sts = UHCIInW(Ctx->HC, UHCI_STS_OFFSET);
    u16 Intr = UHCIInW(Ctx->HC, UHCI_INTR_OFFSET);
    u16 Frame = UHCIInW(Ctx->HC, UHCI_FRNUM_OFFSET);

    DebugStr("  CMD: 0x");
    DebugU64(Cmd);
    DebugStr("\r\n");
    DebugStr("  STS: 0x");
    DebugU64(Sts);
    DebugStr("\r\n");
    DebugStr("  INTR: 0x");
    DebugU64(Intr);
    DebugStr("\r\n");
    DebugStr("  Frame: 0x");
    DebugU64(Frame);
    DebugStr("\r\n");

    for (u8 Port = 0; Port < UHCI_PORT_COUNT; Port++)
    {
        u16 PortSc = UHCIGetPortStatus(Ctx, Port);
        DebugStr("  Port");
        DebugU64(Port);
        DebugStr(": 0x");
        DebugU64(PortSc);
        DebugStr("\r\n");
    }
}

void UHCIDumpRequests(UHCIContext *Ctx)
{
    if (!Ctx)
        return;

    DebugStr("UHCI Pending Requests:\r\n");
    UHCIRequest *Req = Ctx->PendingRequests;
    u64 Count = 0;
    while (Req)
    {
        DebugStr("  Req ");
        DebugU64(Count);
        DebugStr(": Addr=");
        DebugU64(Req->DeviceAddress);
        DebugStr(" EP=");
        DebugU64(Req->Endpoint);
        DebugStr(" Active=");
        DebugU64(Req->Active);
        DebugStr("\r\n");
        Req = Req->Next;
        Count++;
    }

    DebugStr("UHCI Completed Requests:\r\n");
    Req = Ctx->CompletedRequests;
    Count = 0;
    while (Req)
    {
        DebugStr("  Req ");
        DebugU64(Count);
        DebugStr(": Result=");
        DebugU64(Req->Result);
        DebugStr(" Success=");
        DebugU64(Req->Success);
        DebugStr("\r\n");
        Req = Req->Next;
        Count++;
    }
}

void UHCIDumpFrameList(UHCIContext *Ctx, u16 Start, u16 Count)
{
    if (!Ctx || !Ctx->FrameList)
        return;

    DebugStr("UHCI Frame List:\r\n");
    for (u16 I = 0; I < Count && I < UHCI_FRAME_LIST_SIZE; I++)
    {
        u16 Idx = (Start + I) & (UHCI_FRAME_LIST_SIZE - 1);
        u32 Entry = Ctx->FrameList[Idx].Pointer;
        DebugStr("  Frame ");
        DebugU64(Idx);
        DebugStr(": 0x");
        DebugU64(Entry);
        DebugStr("\r\n");
    }
}