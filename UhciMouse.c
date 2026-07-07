#include "UhciMouse.h"
#include "debug.h"

static UHCIResult WaitForRequestComplete(UHCIRequest *req, u32 timeout_us) {
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

static void BuildSetupPacket(u8 *packet, u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength) {
    packet[0] = bmRequestType;
    packet[1] = bRequest;
    packet[2] = wValue & 0xFF;
    packet[3] = (wValue >> 8) & 0xFF;
    packet[4] = wIndex & 0xFF;
    packet[5] = (wIndex >> 8) & 0xFF;
    packet[6] = wLength & 0xFF;
    packet[7] = (wLength >> 8) & 0xFF;
}

static UHCIResult GetDeviceDescriptor(UHCIContext *ctx, u8 devAddr, USBDeviceDescriptor *desc, u8 is_low_speed) {
    u8 setup[8] __attribute__((aligned(16)));
    u8 data8[8] __attribute__((aligned(16)));
    UHCIResult result;

    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0100, 0x0000, 0x0008);

    DebugStr("Setup Packet (8B): ");
    for(int i = 0; i < 8; i++) {
        DebugU8(setup[i]);
        DebugStr(" ");
    }
    DebugChar('\n');

    result = ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, data8, 8, 1, is_low_speed, 8);
    if (result != UHCI_OK) return result;

    u8 maxPacketSize0 = data8[7];
    DebugStr("MaxPacketSize0 = "); DebugU8(maxPacketSize0); DebugChar('\n');

    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0100, 0x0000, 0x0012);

    DebugStr("Setup Packet (18B): ");
    for(int i = 0; i < 8; i++) {
        DebugU8(setup[i]);
        DebugStr(" ");
    }
    DebugChar('\n');

    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, (u8*)desc, 18, 1, is_low_speed, maxPacketSize0);
}

static UHCIResult SetDeviceAddress(UHCIContext *ctx, u8 devAddr, u8 is_low_speed, u8 maxPacketSize) {
    u8 setup[8] __attribute__((aligned(16)));
    BuildSetupPacket(setup, 0x00, 0x05, devAddr, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, 0, 0, setup, 8, NULL, 0, 0, is_low_speed, maxPacketSize);
}

static UHCIResult GetConfigDescriptor(UHCIContext *ctx, u8 devAddr, USBConfigDescriptor *desc, u8 is_low_speed, u8 maxPacketSize) {
    u8 setup[8] __attribute__((aligned(16)));
    u8 data[9] __attribute__((aligned(16)));
    UHCIResult result;
    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, 0x0009);
    result = ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, data, 9, 1, is_low_speed, maxPacketSize);
    if (result == UHCI_OK) MemCopy(desc, data, 9);
    return result;
}

static UHCIResult SetConfiguration(UHCIContext *ctx, u8 devAddr, u8 configValue, u8 is_low_speed, u8 maxPacketSize) {
    u8 setup[8] __attribute__((aligned(16)));
    BuildSetupPacket(setup, 0x00, USB_REQ_SET_CONFIG, configValue, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0, is_low_speed, maxPacketSize);
}

static UHCIResult SetProtocol(UHCIContext *ctx, u8 devAddr, u8 protocol, u8 is_low_speed, u8 maxPacketSize) {
    u8 setup[8] __attribute__((aligned(16)));
    BuildSetupPacket(setup, 0x21, USB_REQ_SET_PROTOCOL, protocol, 0x0000, 0x0000);
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0, is_low_speed, maxPacketSize);
}

static UHCIResult CreateInterruptIN(UHCIContext *ctx, u8 devAddr, u8 endpoint, u8 *buffer, u16 len, u8 is_low_speed, UHCITransferDescriptor **outTD, u32 *outTDPhys) {
    AllocPool *pool = ctx->MemoryPool;
    UHCITransferDescriptor *td;
    u32 tdPhys;
    u32 ctrl_ls = is_low_speed ? UHCI_TD_SPD : 0;

    td = (UHCITransferDescriptor*)MaxAlignedAlloc(pool, sizeof(UHCITransferDescriptor), 16, UHCI_MAX_ADDRESS);
    if (!td) return UHCI_ERROR_NO_MEMORY;
    tdPhys = (u32)(u64)td;
    MemSet(td, 0, sizeof(UHCITransferDescriptor));

    td->Token = UHCI_TD_PID_IN;
    td->Token |= ((devAddr & 0x7F) << 8);
    td->Token |= ((endpoint & 0xF) << 15);
    td->Token |= (0 << 19);
    td->Token |= (((len - 1) & 0x7FF) << 21);

    td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | ctrl_ls | ((len - 1) & 0x7FF);
    td->BufferPointer = (u32)(u64)buffer;
    td->LinkPointer = UHCI_FRAME_TERMINATE;

    *outTD = td;
    *outTDPhys = tdPhys;
    return UHCI_OK;
}

void USBMouseCleanup(USBMouse *mouse) {
    if (!mouse) return;
    if (mouse->PollRequest) {
        if (mouse->PollRequest->QH) {
            UHCIClearFrameListEntry(mouse->UHCI, mouse->PollRequest->QH);
            Free(mouse->UHCI->MemoryPool, mouse->PollRequest->QH);
        }
        if (mouse->PollRequest->TD) {
            Free(mouse->UHCI->MemoryPool, mouse->PollRequest->TD);
        }
        Free(mouse->UHCI->MemoryPool, mouse->PollRequest);
    }
    Free(mouse->UHCI->MemoryPool, mouse);
}

USBMouse* USBMouseInit(UHCIContext *uhci, u8 port) {
    USBMouse *mouse;
    USBDeviceDescriptor devDesc;
    USBConfigDescriptor configDesc;
    u8 setup[8] __attribute__((aligned(16)));
    UHCIResult result;
    u16 currentFrame;
    u32 frameSlot;
    UHCIFrameListEntry *frameList;
    UHCITransferDescriptor *td;
    u32 tdPhys;
    u8 *ptr;
    u8 *end;
    u8 foundEndpoint;
    u16 port_status;
    u8 is_low_speed;
    u32 interval;
    u8 maxPacketSize0;

    if (!uhci || !uhci->MemoryPool) return NULL;
    mouse = (USBMouse*)MaxAlloc(uhci->MemoryPool, sizeof(USBMouse), UHCI_MAX_ADDRESS);
    if (!mouse) return NULL;
    MemSet(mouse, 0, sizeof(USBMouse));
    mouse->UHCI = uhci;

    result = UHCIResetPort(uhci, port);
    if (result != UHCI_OK) return NULL;
    SystemBusySleepMs(100);

    SystemBusySleepMs(100);
    port_status = UHCIGetPortStatus(uhci, port);
    is_low_speed = 1;

    DebugStr("Port "); DebugU8(port);
    DebugStr(" Forced LowSpeed = 1 (status=0x"); DebugU16(port_status); DebugStr(")\n");

    mouse->IsLowSpeed = is_low_speed;

    result = GetDeviceDescriptor(uhci, 0, &devDesc, is_low_speed);
    if (result != UHCI_OK) return NULL;
    maxPacketSize0 = devDesc.bMaxPacketSize0;
    mouse->MaxPacketSize0 = maxPacketSize0;

    if (devDesc.bDeviceClass != 0x00 && devDesc.bDeviceClass != 0x03) return NULL;

    mouse->DeviceAddress = 2;
    result = SetDeviceAddress(uhci, mouse->DeviceAddress, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) return NULL;
    SystemBusySleepMs(10);

    result = GetConfigDescriptor(uhci, mouse->DeviceAddress, &configDesc, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) return NULL;

    result = SetConfiguration(uhci, mouse->DeviceAddress, configDesc.bConfigurationValue, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) return NULL;
    SystemBusySleepMs(10);

    result = SetProtocol(uhci, mouse->DeviceAddress, 0x00, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) return NULL;

    {
        u8 configData[256] __attribute__((aligned(16)));
        USBConfigDescriptor *configHeader = (USBConfigDescriptor*)configData;
        u16 totalLength;

        BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, 255);
        result = ExecuteControlTransfer(uhci, mouse->DeviceAddress, 0, setup, 8, configData, 255, 1, is_low_speed, maxPacketSize0);
        if (result != UHCI_OK) return NULL;
        totalLength = configHeader->wTotalLength;
        if (totalLength > 255) totalLength = 255;
        BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, totalLength);
        result = ExecuteControlTransfer(uhci, mouse->DeviceAddress, 0, setup, 8, configData, totalLength, 1, is_low_speed, maxPacketSize0);
        if (result != UHCI_OK) return NULL;

        ptr = configData;
        end = ptr + totalLength;
        foundEndpoint = 0;
        while (ptr < end) {
            u8 len = ptr[0];
            u8 type = ptr[1];
            if (type == 0x05) {
                USBEndpointDescriptor *ep = (USBEndpointDescriptor*)ptr;
                if ((ep->bEndpointAddress & 0x80) && (ep->bmAttributes & 0x03) == 0x03) {
                    mouse->InterruptEndpoint = ep->bEndpointAddress & 0x0F;
                    mouse->MaxPacketSize = ep->wMaxPacketSize;
                    mouse->InterruptInterval = ep->bInterval;
                    foundEndpoint = 1;
                    break;
                }
            }
            ptr += len;
            if (len == 0) break;
        }
    }
    if (!foundEndpoint) return NULL;

    mouse->PollRequest = UHCICreateRequest(uhci);
    if (!mouse->PollRequest) return NULL;

    result = CreateInterruptIN(uhci, mouse->DeviceAddress, mouse->InterruptEndpoint, mouse->ReportBuffer, USB_MOUSE_REPORT_SIZE, is_low_speed, &td, &tdPhys);
    if (result != UHCI_OK) return NULL;

    UHCIQueueHead *qh = (UHCIQueueHead*)MaxAlignedAlloc(uhci->MemoryPool, sizeof(UHCIQueueHead), 16, UHCI_MAX_ADDRESS);
    if (!qh) return NULL;
    MemSet(qh, 0, sizeof(UHCIQueueHead));

    qh->VerticalLink = (tdPhys & 0xFFFFFFF0) | UHCI_QH_VERTICAL;
    qh->HorizontalLink = UHCI_FRAME_TERMINATE;

    mouse->PollRequest->QH = qh;
    mouse->PollRequest->QHPhys = (u32)(u64)qh;
    mouse->PollRequest->DeviceAddress = mouse->DeviceAddress;
    mouse->PollRequest->Endpoint = mouse->InterruptEndpoint;
    mouse->PollRequest->Type = UHCI_TRANSFER_INTERRUPT;
    mouse->PollRequest->DataBuffer = mouse->ReportBuffer;
    mouse->PollRequest->DataLength = USB_MOUSE_REPORT_SIZE;
    mouse->PollRequest->MaxPacketSize = mouse->MaxPacketSize;
    mouse->PollRequest->TD = td;
    mouse->PollRequest->UHCI = uhci;
    mouse->PollRequest->Completed = 0;
    mouse->PollRequest->Result = UHCI_OK;

    currentFrame = UHCIGetFrameNumber(uhci);
    frameSlot = 0;
    interval = mouse->InterruptInterval;
    if (interval == 0) interval = 10;
    if (interval > 0) frameSlot = currentFrame % interval;
    mouse->PollRequest->FrameIndex = frameSlot;

    frameList = uhci->FrameList;
    if (frameList) {
        for (u32 i = 0; i < 1024; i++) {
            frameList[i].Pointer = UHCI_FRAME_TERMINATE;
        }
        for (u32 i = frameSlot; i < 1024; i += interval) {
            frameList[i].Pointer = ((u32)(u64)qh & 0xFFFFFFF0) | UHCI_FRAME_QH;
        }
    } else {
        return NULL;
    }

    mouse->Polling = 1;
    mouse->Configured = 1;
    return mouse;
}

void USBMousePoll(USBMouse *mouse) {
    UHCITransferDescriptor *td;
    u8 *data;
    u32 controlStatus;
    u32 ctrl_ls;

    if (!mouse || !mouse->Polling || !mouse->PollRequest) return;
    if (mouse->IsPolling) return;
    mouse->IsPolling = 1;

    td = mouse->PollRequest->TD;
    if (!td) {
        mouse->IsPolling = 0;
        return;
    }

    controlStatus = td->ControlStatus;
    ctrl_ls = mouse->IsLowSpeed ? UHCI_TD_SPD : 0;

    if (controlStatus & UHCI_TD_ACTIVE) {
        mouse->IsPolling = 0;
        return;
    }

    if (controlStatus & UHCI_TD_STALL) {
        td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | ctrl_ls | ((USB_MOUSE_REPORT_SIZE - 1) & 0x7FF);
        td->Token &= ~(1 << 19);
        mouse->IsPolling = 0;
        return;
    }

    if (controlStatus & (UHCI_TD_CRCERR | UHCI_TD_BITSTUFF | UHCI_TD_BABBLE | UHCI_TD_DBUFERR)) {
        td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | ctrl_ls | ((USB_MOUSE_REPORT_SIZE - 1) & 0x7FF);
        td->Token &= ~(1 << 19);
        mouse->IsPolling = 0;
        return;
    }

    data = mouse->ReportBuffer;
    mouse->LastReport.Buttons = data[0];
    mouse->LastReport.X = data[1];
    mouse->LastReport.Y = data[2];
    if (USB_MOUSE_REPORT_SIZE >= 4) mouse->LastReport.Wheel = data[3];

    td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | ctrl_ls | ((USB_MOUSE_REPORT_SIZE - 1) & 0x7FF);
    td->Token ^= (1 << 19);
    mouse->IsPolling = 0;
}

u8 USBMouseGetButtons(USBMouse *mouse) {
    if (!mouse) return 0;
    return mouse->LastReport.Buttons;
}

s8 USBMouseGetX(USBMouse *mouse) {
    if (!mouse) return 0;
    return (s8)mouse->LastReport.X;
}

s8 USBMouseGetY(USBMouse *mouse) {
    if (!mouse) return 0;
    return (s8)mouse->LastReport.Y;
}

s8 USBMouseGetWheel(USBMouse *mouse) {
    if (!mouse) return 0;
    return (s8)mouse->LastReport.Wheel;
}