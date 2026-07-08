#include "UhciMouse.h"
#include "debug.h"
#include "uhci.h"

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
    DebugStr("USBMouseInit start on port ");
    DebugU8(port);
    DebugStr("\n");
    
    USBMouse *mouse;
    USBDeviceDescriptor devDesc;
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
    u8 foundMouseInterface;
    u16 port_status;
    u8 is_low_speed;
    u32 interval;
    u8 maxPacketSize0;
    u8 configValue;
    if (!uhci || !uhci->MemoryPool) return NULL;
    mouse = (USBMouse*)MaxAlloc(uhci->MemoryPool, sizeof(USBMouse), UHCI_MAX_ADDRESS);
    if (!mouse) return NULL;
    MemSet(mouse, 0, sizeof(USBMouse));
    mouse->UHCI = uhci;
    SystemBusySleepMs(100);
    port_status = UHCIGetPortStatus(uhci, port);
    is_low_speed = (port_status & UHCI_PORTSC_LSDA) ? 1 : 0;
    DebugStr("Port "); DebugU8(port);
    DebugStr(" LowSpeed = "); DebugU8(is_low_speed);
    DebugStr(" (status=0x"); DebugU16(port_status); DebugStr(")\n");
    mouse->IsLowSpeed = is_low_speed;
    result = GetDeviceDescriptor(uhci, 0, &devDesc, is_low_speed);
    if (result != UHCI_OK) goto cleanup;
    maxPacketSize0 = devDesc.bMaxPacketSize0;
    mouse->MaxPacketSize0 = maxPacketSize0;
    mouse->DeviceAddress = 2;
    result = SetDeviceAddress(uhci, mouse->DeviceAddress, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) goto cleanup;
    SystemBusySleepMs(10);
    {
        u8 configData[256] __attribute__((aligned(16)));
        u16 totalLength;
        BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, 255);
        result = ExecuteControlTransfer(uhci, mouse->DeviceAddress, 0, setup, 8, configData, 255, 1, is_low_speed, maxPacketSize0);
        if (result != UHCI_OK) goto cleanup;
        totalLength = configData[2] | (configData[3] << 8);
        if (totalLength > 255) totalLength = 255;
        MemSet(configData, 0, 256);
        BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, totalLength);
        result = ExecuteControlTransfer(uhci, mouse->DeviceAddress, 0, setup, 8, configData, totalLength, 1, is_low_speed, maxPacketSize0);
        if (result != UHCI_OK) goto cleanup;
        configValue = configData[5];
        ptr = configData;
        end = ptr + totalLength;
        foundEndpoint = 0;
        foundMouseInterface = 0;
        while (ptr < end) {
            u8 len = ptr[0];
            u8 type = ptr[1];
            if (type == 0x04) {
                if (ptr[5] == 0x03 && ptr[6] == 0x01 && ptr[7] == 0x02) {
                    foundMouseInterface = 1;
                } else {
                    foundMouseInterface = 0;
                }
            }
            if (type == 0x05 && foundMouseInterface) {
                if ((ptr[2] & 0x80) && (ptr[3] & 0x03) == 0x03) {
                    mouse->InterruptEndpoint = ptr[2] & 0x0F;
                    mouse->MaxPacketSize = ptr[4] | (ptr[5] << 8);
                    mouse->InterruptInterval = ptr[6];
                    foundEndpoint = 1;
                    break;
                }
            }
            ptr += len;
            if (len == 0) break;
        }
    }
    if (!foundEndpoint) goto cleanup;
    result = SetConfiguration(uhci, mouse->DeviceAddress, configValue, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) goto cleanup;
    SystemBusySleepMs(10);
    result = SetProtocol(uhci, mouse->DeviceAddress, 0x00, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) goto cleanup;
    mouse->PollRequest = UHCICreateRequest(uhci);
    if (!mouse->PollRequest) goto cleanup;
    result = CreateInterruptIN(uhci, mouse->DeviceAddress, mouse->InterruptEndpoint, mouse->ReportBuffer, USB_MOUSE_REPORT_SIZE, is_low_speed, &td, &tdPhys);
    if (result != UHCI_OK) goto cleanup;
    UHCIQueueHead *qh = (UHCIQueueHead*)MaxAlignedAlloc(uhci->MemoryPool, sizeof(UHCIQueueHead), 16, UHCI_MAX_ADDRESS);
    if (!qh) goto cleanup;
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
    interval = mouse->InterruptInterval;
    if (interval == 0) interval = 10;
    frameSlot = currentFrame % interval;
    mouse->PollRequest->FrameIndex = frameSlot;

    frameList = uhci->FrameList;
    if (frameList) {
        for (u32 i = frameSlot; i < 1024; i += interval) {
            frameList[i].Pointer = ((u32)(u64)qh & 0xFFFFFFF0) | UHCI_FRAME_QH;
        }
    } else {
        goto cleanup;
    }

    UHCISubmitRequest(uhci, mouse->PollRequest);

    mouse->Polling = 1;
    mouse->Configured = 1;
    return mouse;

cleanup:
    if (mouse) {
        if (mouse->PollRequest) {
            if (mouse->PollRequest->QH) {
                UHCIClearFrameListEntry(uhci, mouse->PollRequest->QH);
                Free(uhci->MemoryPool, mouse->PollRequest->QH);
            }
            if (mouse->PollRequest->TD) {
                Free(uhci->MemoryPool, mouse->PollRequest->TD);
            }
            Free(uhci->MemoryPool, mouse->PollRequest);
        }
        Free(uhci->MemoryPool, mouse);
    }
    if (uhci) {
        UHCIResetPort(uhci, port);
    }
    return NULL;
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
    ctrl_ls = mouse->IsLowSpeed ? UHCI_TD_LS : 0;
    if (controlStatus & UHCI_TD_ACTIVE) {
        mouse->IsPolling = 0;
        return;
    }
    if (controlStatus & UHCI_TD_STALL) {
        td->ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((USB_MOUSE_REPORT_SIZE - 1) & 0x7FF);
        td->Token &= ~(1 << 19);
        mouse->IsPolling = 0;
        return;
    }
    if (controlStatus & (UHCI_TD_CRCERR | UHCI_TD_BITSTUFF | UHCI_TD_BABBLE | UHCI_TD_DBUFERR)) {
        td->ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((USB_MOUSE_REPORT_SIZE - 1) & 0x7FF);
        td->Token &= ~(1 << 19);
        mouse->IsPolling = 0;
        return;
    }
    data = mouse->ReportBuffer;
    mouse->LastReport.Buttons = data[0];
    mouse->LastReport.X = data[1];
    mouse->LastReport.Y = data[2];
    if (USB_MOUSE_REPORT_SIZE >= 4) mouse->LastReport.Wheel = data[3];
    td->ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((USB_MOUSE_REPORT_SIZE - 1) & 0x7FF);
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