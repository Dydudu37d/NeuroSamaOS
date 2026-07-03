#include "UhciMouse.h"
#include "debug.h"
#include "str.h"

static UHCIResult WaitForRequestComplete(UHCIRequest *req, u32 timeout_us)
{
    u32 elapsed = 0;
    while (!req->Completed) {
        UHCIPoll(req->UHCI);
        for (u32 i = 0; i < 50; i++) __asm__ volatile("pause");
        elapsed += 500;
        if (elapsed > timeout_us) return UHCI_ERROR_TIMEOUT;
    }
    return req->Result;
}

static void BuildSetupPacket(u8 *packet, u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength)
{
    packet[0] = bmRequestType;
    packet[1] = bRequest;
    *(u16*)(packet+2) = wValue;
    *(u16*)(packet+4) = wIndex;
    *(u16*)(packet+6) = wLength;
}

static UHCIResult ExecuteControlTransfer(UHCIContext *ctx, u8 devAddr, u8 endpoint, u8 *setupPacket, u16 setupLen, u8 *data, u16 dataLen, u8 dir)
{
    UHCIRequest *req = UHCICreateRequest(ctx);
    if (!req) return UHCI_ERROR_NO_MEMORY;
    USBControlQueue *q = Alloc(ctx->MemoryPool, sizeof(USBControlQueue));
    if (!q) {
        Free(ctx->MemoryPool, req);
        return UHCI_ERROR_NO_MEMORY;
    }
    MemSet(q, 0, sizeof(USBControlQueue));
    u32 setupPid = (UHCI_TD_PID_SETUP << 24);
    u32 inPid = (UHCI_TD_PID_IN << 24);
    u32 outPid = (UHCI_TD_PID_OUT << 24);
    u32 devAddrField = (devAddr << UHCI_TD_TOKEN_DEVADDR_SHIFT) & UHCI_TD_TOKEN_DEVADDR_MASK;
    u32 endpField = (endpoint << UHCI_TD_TOKEN_ENDPT_SHIFT) & UHCI_TD_TOKEN_ENDPT_MASK;
    q->tds[0].Token = setupPid | devAddrField | endpField | (((setupLen - 1) & 0x7FF) << 21);
    q->tds[0].ControlStatus = UHCI_TD_ACTIVE | (3 << 27);
    q->tds[0].BufferPointer = (u32)(u64)setupPacket;
    q->tds[0].LinkPointer = (u32)(u64)&q->tds[1] | 0x04;
    if (dataLen > 0) {
        if (dir) {
            q->tds[1].Token = inPid | devAddrField | endpField | (((dataLen - 1) & 0x7FF) << 21) | (1 << 19);
        } else {
            q->tds[1].Token = outPid | devAddrField | endpField | (((dataLen - 1) & 0x7FF) << 21) | (1 << 19);
        }
        q->tds[1].ControlStatus = UHCI_TD_ACTIVE | (3 << 27);
        q->tds[1].BufferPointer = (u32)(u64)data;
        q->tds[1].LinkPointer = (u32)(u64)&q->tds[2] | 0x04;
    } else {
        q->tds[0].LinkPointer = (u32)(u64)&q->tds[2] | 0x04;
    }
    u32 maxLenStatus = 0x7FF << 21;
    if (dir || dataLen == 0) {
        q->tds[2].Token = outPid | devAddrField | endpField | maxLenStatus | (1 << 19);
    } else {
        q->tds[2].Token = inPid | devAddrField | endpField | maxLenStatus | (1 << 19);
    }
    q->tds[2].ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | (3 << 27);
    q->tds[2].BufferPointer = 0;
    q->tds[2].LinkPointer = UHCI_FRAME_TERMINATE;
    q->qh.VerticalLink = (u32)(u64)&q->tds[0] | 0x00;
    u16 FrameIdx = UHCIGetFrameNumber(ctx) & (UHCI_FRAME_LIST_SIZE - 1);
    u32 oldFrameHead = ctx->FrameList[FrameIdx].Pointer;
    q->qh.HorizontalLink = oldFrameHead;
    ctx->FrameList[FrameIdx].Pointer = (u32)(u64)&q->qh | 0x02;
    req->DeviceAddress = devAddr;
    req->Endpoint = endpoint;
    req->Direction = dir ? UHCI_TRANSFER_IN : UHCI_TRANSFER_OUT;
    req->Type = UHCI_TRANSFER_CONTROL;
    req->DataBuffer = data;
    req->DataLength = dataLen;
    req->MaxPacketSize = 8;
    req->TDList = (UHCITransferDescriptor*)q->tds;
    req->TDCount = 3;
    req->QH = &q->qh;
    req->UHCI = ctx;
    req->Next = ctx->PendingRequests;
    ctx->PendingRequests = req;
    UHCIResult result = WaitForRequestComplete(req, USB_CTRL_TIMEOUT);
    UHCIRequest *prev = NULL;
    UHCIRequest *cur = ctx->PendingRequests;
    while (cur) {
        if (cur == req) {
            if (prev) prev->Next = cur->Next;
            else ctx->PendingRequests = cur->Next;
            break;
        }
        prev = cur;
        cur = cur->Next;
    }
    ctx->FrameList[FrameIdx].Pointer = oldFrameHead;
    Free(ctx->MemoryPool, q);
    Free(ctx->MemoryPool, req);
    return result;
}

static UHCIResult GetDeviceDescriptor(UHCIContext *ctx, u8 devAddr, USBDeviceDescriptor *desc)
{
    u8 setup[8];
    u8 data[18];
    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0100, 0x0000, 0x12);
    UHCIResult result = ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, data, 18, 1);
    if (result != UHCI_OK) return result;
    MemCopy(desc, data, sizeof(USBDeviceDescriptor));
    return UHCI_OK;
}

static UHCIResult SetDeviceAddress(UHCIContext *ctx, u8 devAddr)
{
    u8 setup[8];
    BuildSetupPacket(setup, 0x00, 0x05, devAddr, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, 0, 0, setup, 8, NULL, 0, 0);
}

static UHCIResult GetConfigDescriptor(UHCIContext *ctx, u8 devAddr, USBConfigDescriptor *desc)
{
    u8 setup[8];
    u8 data[9];
    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, 0x09);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, data, 9, 1);
}

static UHCIResult SetConfiguration(UHCIContext *ctx, u8 devAddr, u8 configValue)
{
    u8 setup[8];
    BuildSetupPacket(setup, 0x00, USB_REQ_SET_CONFIG, configValue, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0);
}

static UHCIResult SetIdle(UHCIContext *ctx, u8 devAddr, u8 duration)
{
    u8 setup[8];
    BuildSetupPacket(setup, 0x21, USB_REQ_SET_IDLE, (duration << 8), 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0);
}

static UHCIResult SetProtocol(UHCIContext *ctx, u8 devAddr, u8 protocol)
{
    u8 setup[8];
    BuildSetupPacket(setup, 0x21, USB_REQ_SET_PROTOCOL, protocol, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0);
}

USBMouse *USBMouseInit(UHCIContext *uhci, u8 port)
{
    if (!uhci || !uhci->MemoryPool) return NULL;
    USBMouse *mouse = Alloc(uhci->MemoryPool, sizeof(USBMouse));
    if (!mouse) return NULL;
    MemSet(mouse, 0, sizeof(USBMouse));
    mouse->UHCI = uhci;
    UHCIResetPort(uhci, port);
    UHCIClearPortChange(uhci, port);
    SystemBusySleepMs(300);
    USBDeviceDescriptor devDesc;
    if (GetDeviceDescriptor(uhci, 0, &devDesc) != UHCI_OK) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    mouse->DeviceAddress = 1;
    if (SetDeviceAddress(uhci, mouse->DeviceAddress) != UHCI_OK) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    SystemBusySleepMs(50);
    USBConfigDescriptor configDesc;
    if (GetConfigDescriptor(uhci, mouse->DeviceAddress, &configDesc) != UHCI_OK) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    if (SetConfiguration(uhci, mouse->DeviceAddress, configDesc.bConfigurationValue) != UHCI_OK) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    SystemBusySleepMs(50);
    SetProtocol(uhci, mouse->DeviceAddress, USB_MOUSE_PROTOCOL_BOOT);
    SetIdle(uhci, mouse->DeviceAddress, 0);
    mouse->InterruptEndpoint = 1;
    mouse->MaxPacketSize = 8;
    mouse->PollRequest = UHCICreateRequest(uhci);
    if (!mouse->PollRequest) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    UHCITransferDescriptor *td = (UHCITransferDescriptor*)AlignedAlloc(uhci->MemoryPool, sizeof(UHCITransferDescriptor), 16);
    if (!td) {
        Free(uhci->MemoryPool, mouse->PollRequest);
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    MemSet(td, 0, sizeof(UHCITransferDescriptor));
    td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | (3 << 27);
    td->Token = (UHCI_TD_PID_IN << 24) | (mouse->DeviceAddress << UHCI_TD_TOKEN_DEVADDR_SHIFT) | (mouse->InterruptEndpoint << UHCI_TD_TOKEN_ENDPT_SHIFT) | ((mouse->MaxPacketSize - 1) << 21) | (1 << 19);
    td->BufferPointer = (u32)(u64)mouse->ReportBuffer;
    td->LinkPointer = UHCI_FRAME_TERMINATE;
    mouse->PollRequest->TD = td;
    mouse->PollRequest->DataBuffer = mouse->ReportBuffer;
    mouse->PollRequest->DataLength = USB_MOUSE_REPORT_SIZE;
    mouse->PollRequest->DeviceAddress = mouse->DeviceAddress;
    mouse->PollRequest->Endpoint = mouse->InterruptEndpoint;
    mouse->PollRequest->Type = UHCI_TRANSFER_INTERRUPT;
    mouse->Polling = 1;
    mouse->Configured = 1;
    return mouse;
}

void USBMousePoll(USBMouse *mouse)
{
    if (!mouse || !mouse->Polling || !mouse->PollRequest) return;
    UHCITransferDescriptor *td = mouse->PollRequest->TD;
    if (!td) return;
    if (!(td->ControlStatus & UHCI_TD_ACTIVE)) {
        if (td->ControlStatus & UHCI_TD_STALL) {
            td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | (3 << 27);
            td->Token ^= (1 << 19);
            return;
        }
        u8 *data = mouse->ReportBuffer;
        mouse->LastReport.Buttons = data[0] & 0x07;
        mouse->LastReport.X = (s8)data[1];
        mouse->LastReport.Y = (s8)data[2];
        mouse->LastReport.Wheel = (s8)data[3];
        td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | (3 << 27);
        td->Token ^= (1 << 19);
    }
}

u8 USBMouseGetButtons(USBMouse *mouse) { if (!mouse) return 0; return mouse->LastReport.Buttons; }
s8 USBMouseGetX(USBMouse *mouse) { if (!mouse) return 0; return mouse->LastReport.X; }
s8 USBMouseGetY(USBMouse *mouse) { if (!mouse) return 0; return mouse->LastReport.Y; }
s8 USBMouseGetWheel(USBMouse *mouse) { if (!mouse) return 0; return mouse->LastReport.Wheel; }