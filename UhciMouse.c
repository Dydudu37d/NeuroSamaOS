#include "UhciMouse.h"
#include "debug.h"
#include "clock.h"
#include "str.h"

static UHCIResult WaitForRequestComplete(UHCIRequest *req, u32 timeout_us)
{
    u32 elapsed = 0;
    
    while (!req->Completed) {
        UHCIPoll(req->UHCI);
        for (u32 i = 0; i < 100; i++) {
            __asm__ volatile ("pause");
        }
        elapsed += 100;
        if (elapsed > timeout_us) {
            return UHCI_ERROR_TIMEOUT;
        }
    }
    
    return req->Result;
}

static void BuildSetupPacket(u8 *packet, u8 bmRequestType, u8 bRequest,
                              u16 wValue, u16 wIndex, u16 wLength)
{
    packet[0] = bmRequestType;
    packet[1] = bRequest;
    packet[2] = wValue & 0xFF;
    packet[3] = (wValue >> 8) & 0xFF;
    packet[4] = wIndex & 0xFF;
    packet[5] = (wIndex >> 8) & 0xFF;
    packet[6] = wLength & 0xFF;
    packet[7] = (wLength >> 8) & 0xFF;
}

static UHCIResult ExecuteControlTransfer(UHCIContext *ctx,
                                          u8 devAddr, u8 endpoint,
                                          u8 *setupPacket, u16 setupLen,
                                          u8 *data, u16 dataLen, u8 dir)
{
    UHCIRequest *req;
    USBControlQueue *q;
    UHCIResult result;
    u16 maxPacket = 8;
    
    req = UHCICreateRequest(ctx);
    if (!req) return UHCI_ERROR_NO_MEMORY;
    
    q = Alloc(ctx->MemoryPool, sizeof(USBControlQueue));
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
    
    q->tds[0].Token = setupPid | devAddrField | endpField | (setupLen & 0x7FF);
    q->tds[0].ControlStatus = UHCI_TD_ACTIVE | (setupLen & UHCI_TD_MAXLEN_MASK);
    q->tds[0].BufferPointer = (u32)(u64)setupPacket;
    q->tds[0].LinkPointer = (u32)(u64)&q->tds[1];
    
    if (dir) {
        q->tds[1].Token = inPid | devAddrField | endpField | (dataLen & 0x7FF);
    } else {
        q->tds[1].Token = outPid | devAddrField | endpField | (dataLen & 0x7FF);
    }
    q->tds[1].ControlStatus = UHCI_TD_ACTIVE | (dataLen & UHCI_TD_MAXLEN_MASK);
    q->tds[1].BufferPointer = (u32)(u64)data;
    q->tds[1].LinkPointer = (u32)(u64)&q->tds[2];
    
    if (dir) {
        q->tds[2].Token = outPid | devAddrField | endpField;
    } else {
        q->tds[2].Token = inPid | devAddrField | endpField;
    }
    q->tds[2].ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC;
    q->tds[2].BufferPointer = 0;
    q->tds[2].LinkPointer = UHCI_FRAME_TERMINATE;
    
    q->qh.VerticalLink = (u32)(u64)&q->tds[0] | 0x02;
    q->qh.HorizontalLink = UHCI_FRAME_TERMINATE;
    
    req->DeviceAddress = devAddr;
    req->Endpoint = endpoint;
    req->Direction = dir ? UHCI_TRANSFER_IN : UHCI_TRANSFER_OUT;
    req->Type = UHCI_TRANSFER_CONTROL;
    req->DataBuffer = data;
    req->DataLength = dataLen;
    req->MaxPacketSize = maxPacket;
    req->TDList = (UHCITransferDescriptor*)q->tds;
    req->TDCount = 3;
    req->QH = &q->qh;
    req->UHCI = ctx;
    
    result = UHCISubmitRequest(ctx, req);
    if (result != UHCI_OK) {
        Free(ctx->MemoryPool, q);
        Free(ctx->MemoryPool, req);
        return result;
    }
    
    result = WaitForRequestComplete(req, USB_CTRL_TIMEOUT);
    
    Free(ctx->MemoryPool, q);
    Free(ctx->MemoryPool, req);
    
    return result;
}

static UHCIResult GetDeviceDescriptor(UHCIContext *ctx, u8 devAddr,
                                       USBDeviceDescriptor *desc)
{
    u8 setup[8];
    u8 data[18];
    UHCIResult result;
    
    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0100, 0x0000, 0x12);
    
    result = ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, data, 18, 1);
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

static UHCIResult GetConfigDescriptor(UHCIContext *ctx, u8 devAddr,
                                       USBConfigDescriptor *desc)
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

static UHCIResult CreateInterruptIN(UHCIContext *ctx, u8 devAddr, u8 endpoint,
                                     u8 *buffer, u16 len,
                                     UHCITransferDescriptor **outTD,
                                     u32 *outTDPhys)
{
    AllocPool *pool = ctx->MemoryPool;
    UHCITransferDescriptor *td;
    u32 tdPhys;
    
    td = Alloc(pool, sizeof(UHCITransferDescriptor));
    if (!td) return UHCI_ERROR_NO_MEMORY;
    tdPhys = (u32)(u64)td;
    
    MemSet(td, 0, sizeof(UHCITransferDescriptor));
    
    td->Token = (UHCI_TD_PID_IN << 24);
    td->Token |= (devAddr << UHCI_TD_TOKEN_DEVADDR_SHIFT) & UHCI_TD_TOKEN_DEVADDR_MASK;
    td->Token |= (endpoint << UHCI_TD_TOKEN_ENDPT_SHIFT) & UHCI_TD_TOKEN_ENDPT_MASK;
    td->Token |= (len & 0x7FF);
    td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | (len & UHCI_TD_MAXLEN_MASK);
    td->BufferPointer = (u32)(u64)buffer;
    td->LinkPointer = UHCI_FRAME_TERMINATE;
    
    *outTD = td;
    *outTDPhys = tdPhys;
    
    return UHCI_OK;
}

USBMouse *USBMouseInit(UHCIContext *uhci, u8 port)
{
    USBMouse *mouse;
    USBDeviceDescriptor devDesc;
    USBConfigDescriptor configDesc;
    u8 setup[8];
    UHCIResult result;
    u16 currentFrame;
    u32 frameSlot;
    UHCIFrameListEntry *frameList;
    UHCITransferDescriptor *td;
    u32 tdPhys;
    u8 *ptr;
    u8 *end;
    u8 foundEndpoint;
    
    if (!uhci || !uhci->MemoryPool) return NULL;
    
    mouse = Alloc(uhci->MemoryPool, sizeof(USBMouse));
    if (!mouse) return NULL;
    MemSet(mouse, 0, sizeof(USBMouse));
    mouse->UHCI = uhci;
    
    result = UHCIResetPort(uhci, port);
    if (result != UHCI_OK) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    
    SystemBusySleepMs(100);
    
    result = GetDeviceDescriptor(uhci, 0, &devDesc);
    if (result != UHCI_OK) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    
    if (devDesc.bDeviceClass != 0x00 && devDesc.bDeviceClass != 0x03) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    
    mouse->DeviceAddress = 1;
    
    result = SetDeviceAddress(uhci, mouse->DeviceAddress);
    if (result != UHCI_OK) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    SystemBusySleepMs(10);
    
    result = GetConfigDescriptor(uhci, mouse->DeviceAddress, &configDesc);
    if (result != UHCI_OK) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    
    result = SetConfiguration(uhci, mouse->DeviceAddress, 
                               configDesc.bConfigurationValue);
    if (result != UHCI_OK) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    SystemBusySleepMs(10);
    
    result = SetProtocol(uhci, mouse->DeviceAddress, USB_MOUSE_PROTOCOL_BOOT);
    if (result != UHCI_OK) {
        result = SetProtocol(uhci, mouse->DeviceAddress, USB_MOUSE_PROTOCOL_REPORT);
        if (result != UHCI_OK) {
            Free(uhci->MemoryPool, mouse);
            return NULL;
        }
    }
    
    result = SetIdle(uhci, mouse->DeviceAddress, 0);
    if (result != UHCI_OK) {
    }
    
    {
        u8 configData[256];
        BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, 255);
        result = ExecuteControlTransfer(uhci, mouse->DeviceAddress, 0,
                                         setup, 8,
                                         configData, 255, 1);
        if (result != UHCI_OK) {
            Free(uhci->MemoryPool, mouse);
            return NULL;
        }
        
        ptr = configData;
        end = ptr + ((USBConfigDescriptor*)ptr)->wTotalLength;
        foundEndpoint = 0;
        
        while (ptr < end) {
            u8 len = ptr[0];
            u8 type = ptr[1];
            
            if (type == 0x05) {
                USBEndpointDescriptor *ep = (USBEndpointDescriptor*)ptr;
                if ((ep->bEndpointAddress & 0x80) && 
                    (ep->bmAttributes & 0x03) == 0x03) {
                    mouse->InterruptEndpoint = ep->bEndpointAddress & 0x0F;
                    mouse->MaxPacketSize = ep->wMaxPacketSize;
                    mouse->InterruptInterval = ep->bInterval;
                    foundEndpoint = 1;
                    break;
                }
            }
            ptr += len;
        }
    }
    
    if (!foundEndpoint) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    
    mouse->PollRequest = UHCICreateRequest(uhci);
    if (!mouse->PollRequest) {
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    
    result = CreateInterruptIN(uhci, mouse->DeviceAddress,
                                mouse->InterruptEndpoint,
                                mouse->ReportBuffer,
                                USB_MOUSE_REPORT_SIZE,
                                &td, &tdPhys);
    if (result != UHCI_OK) {
        Free(uhci->MemoryPool, mouse->PollRequest);
        Free(uhci->MemoryPool, mouse);
        return NULL;
    }
    
    mouse->PollRequest->DeviceAddress = mouse->DeviceAddress;
    mouse->PollRequest->Endpoint = mouse->InterruptEndpoint;
    mouse->PollRequest->Type = UHCI_TRANSFER_INTERRUPT;
    mouse->PollRequest->DataBuffer = mouse->ReportBuffer;
    mouse->PollRequest->DataLength = USB_MOUSE_REPORT_SIZE;
    mouse->PollRequest->MaxPacketSize = mouse->MaxPacketSize;
    mouse->PollRequest->TD = td;
    mouse->PollRequest->UHCI = uhci;
    mouse->PollRequest->TDCount = 1;
    
    currentFrame = UHCIGetFrameNumber(uhci);
    frameSlot = currentFrame % UHCI_FRAME_LIST_SIZE;
    
    frameList = (UHCIFrameListEntry*)uhci->FrameListVirt;
    frameList[frameSlot].Pointer = tdPhys | 0x02;
    
    mouse->Polling = 1;
    mouse->Configured = 1;
    
    return mouse;
}

void USBMousePoll(USBMouse *mouse)
{
    UHCITransferDescriptor *td;
    u8 *data;
    
    if (!mouse || !mouse->Polling) return;
    
    td = mouse->PollRequest->TD;
    
    if (!(td->ControlStatus & UHCI_TD_ACTIVE)) {
        if (td->ControlStatus & UHCI_TD_STALL) {
            td->ControlStatus = UHCI_TD_ACTIVE | USB_MOUSE_REPORT_SIZE;
            return;
        }
        
        data = mouse->ReportBuffer;
        
        mouse->LastReport.Buttons = data[0] & 0x07;
        mouse->LastReport.X = (s8)data[1];
        mouse->LastReport.Y = (s8)data[2];
        mouse->LastReport.Wheel = (s8)data[3];
        
        td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | USB_MOUSE_REPORT_SIZE;
    }
}

u8 USBMouseGetButtons(USBMouse *mouse)
{
    if (!mouse) return 0;
    return mouse->LastReport.Buttons;
}

s8 USBMouseGetX(USBMouse *mouse)
{
    if (!mouse) return 0;
    return mouse->LastReport.X;
}

s8 USBMouseGetY(USBMouse *mouse)
{
    if (!mouse) return 0;
    return mouse->LastReport.Y;
}

s8 USBMouseGetWheel(USBMouse *mouse)
{
    if (!mouse) return 0;
    return mouse->LastReport.Wheel;
}

void USBMouseTest(UHCIContext *uhci)
{
    USBMouse *mouse;
    int i;
    
    mouse = USBMouseInit(uhci, 0);
    if (!mouse) {
        return;
    }
    
    for (i = 0; i < 1000; i++) {
        USBMousePoll(mouse);
        SystemBusySleepMs(10);
    }
}