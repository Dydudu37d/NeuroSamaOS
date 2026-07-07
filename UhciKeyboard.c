#include "UhciKeyboard.h"
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

static UHCIResult SetIdle(UHCIContext *ctx, u8 devAddr, u8 duration, u8 is_low_speed, u8 maxPacketSize) {
    u8 setup[8] __attribute__((aligned(16)));
    BuildSetupPacket(setup, 0x21, USB_REQ_SET_IDLE, (duration << 8), 0x0000, 0x0000);
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

void USBKeyboardCleanup(USBKeyboard *keyboard) {
    if (!keyboard) return;
    if (keyboard->PollRequest) {
        if (keyboard->PollRequest->QH) {
            UHCIClearFrameListEntry(keyboard->UHCI, keyboard->PollRequest->QH);
            Free(keyboard->UHCI->MemoryPool, keyboard->PollRequest->QH);
        }
        if (keyboard->PollRequest->TD) {
            Free(keyboard->UHCI->MemoryPool, keyboard->PollRequest->TD);
        }
        Free(keyboard->UHCI->MemoryPool, keyboard->PollRequest);
    }
    Free(keyboard->UHCI->MemoryPool, keyboard);
}

USBKeyboard* USBKeyboardInit(UHCIContext *uhci, u8 port) {
    USBKeyboard *keyboard;
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
    u8 assignedAddr;
    u8 maxPacketSize0;

    if (!uhci || !uhci->MemoryPool) return NULL;
    keyboard = (USBKeyboard*)MaxAlloc(uhci->MemoryPool, sizeof(USBKeyboard), UHCI_MAX_ADDRESS);
    if (!keyboard) return NULL;
    MemSet(keyboard, 0, sizeof(USBKeyboard));
    keyboard->UHCI = uhci;

    result = UHCIResetPort(uhci, port);
    if (result != UHCI_OK) return NULL;
    SystemBusySleepMs(100);

    SystemBusySleepMs(100);
    port_status = UHCIGetPortStatus(uhci, port);
    is_low_speed = 1;

    DebugStr("Port "); DebugU8(port);
    DebugStr(" Forced LowSpeed = 1 (status=0x"); DebugU16(port_status); DebugStr(")\n");

    keyboard->IsLowSpeed = is_low_speed;

    result = GetDeviceDescriptor(uhci, 0, &devDesc, is_low_speed);
    if (result != UHCI_OK) return NULL;
    maxPacketSize0 = devDesc.bMaxPacketSize0;
    keyboard->MaxPacketSize0 = maxPacketSize0;

    if (devDesc.bDeviceClass != 0x00 && devDesc.bDeviceClass != 0x03) return NULL;

    assignedAddr = 1;
    keyboard->DeviceAddress = assignedAddr;
    result = SetDeviceAddress(uhci, assignedAddr, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) return NULL;
    SystemBusySleepMs(10);

    result = GetConfigDescriptor(uhci, keyboard->DeviceAddress, &configDesc, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) return NULL;

    result = SetConfiguration(uhci, keyboard->DeviceAddress, configDesc.bConfigurationValue, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) return NULL;
    SystemBusySleepMs(10);

    result = SetProtocol(uhci, keyboard->DeviceAddress, USB_KEYBOARD_PROTOCOL_BOOT, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) {
        result = SetProtocol(uhci, keyboard->DeviceAddress, USB_KEYBOARD_PROTOCOL_REPORT, is_low_speed, maxPacketSize0);
        if (result != UHCI_OK) return NULL;
    }

    result = SetIdle(uhci, keyboard->DeviceAddress, 0, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) {}

    {
        u8 configData[256] __attribute__((aligned(16)));
        USBConfigDescriptor *configHeader = (USBConfigDescriptor*)configData;
        u16 totalLength;

        BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, 255);
        result = ExecuteControlTransfer(uhci, keyboard->DeviceAddress, 0, setup, 8, configData, 255, 1, is_low_speed, maxPacketSize0);
        if (result != UHCI_OK) return NULL;
        totalLength = configHeader->wTotalLength;
        if (totalLength > 255) totalLength = 255;
        BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, totalLength);
        result = ExecuteControlTransfer(uhci, keyboard->DeviceAddress, 0, setup, 8, configData, totalLength, 1, is_low_speed, maxPacketSize0);
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
                    keyboard->InterruptEndpoint = ep->bEndpointAddress & 0x0F;
                    keyboard->MaxPacketSize = ep->wMaxPacketSize;
                    keyboard->InterruptInterval = ep->bInterval;
                    foundEndpoint = 1;
                    break;
                }
            }
            ptr += len;
            if (len == 0) break;
        }
    }
    if (!foundEndpoint) return NULL;

    keyboard->PollRequest = UHCICreateRequest(uhci);
    if (!keyboard->PollRequest) return NULL;

    result = CreateInterruptIN(uhci, keyboard->DeviceAddress, keyboard->InterruptEndpoint, keyboard->ReportBuffer, USB_KEYBOARD_REPORT_SIZE, is_low_speed, &td, &tdPhys);
    if (result != UHCI_OK) return NULL;

    UHCIQueueHead *qh = (UHCIQueueHead*)MaxAlignedAlloc(uhci->MemoryPool, sizeof(UHCIQueueHead), 16, UHCI_MAX_ADDRESS);
    if (!qh) return NULL;
    MemSet(qh, 0, sizeof(UHCIQueueHead));

    qh->VerticalLink = (tdPhys & 0xFFFFFFF0) | UHCI_QH_VERTICAL;
    qh->HorizontalLink = UHCI_FRAME_TERMINATE;

    keyboard->PollRequest->QH = qh;
    keyboard->PollRequest->QHPhys = (u32)(u64)qh;
    keyboard->PollRequest->DeviceAddress = keyboard->DeviceAddress;
    keyboard->PollRequest->Endpoint = keyboard->InterruptEndpoint;
    keyboard->PollRequest->Type = UHCI_TRANSFER_INTERRUPT;
    keyboard->PollRequest->DataBuffer = keyboard->ReportBuffer;
    keyboard->PollRequest->DataLength = USB_KEYBOARD_REPORT_SIZE;
    keyboard->PollRequest->MaxPacketSize = keyboard->MaxPacketSize;
    keyboard->PollRequest->TD = td;
    keyboard->PollRequest->UHCI = uhci;
    keyboard->PollRequest->Completed = 0;
    keyboard->PollRequest->Result = UHCI_OK;

    currentFrame = UHCIGetFrameNumber(uhci);
    frameSlot = 0;
    interval = keyboard->InterruptInterval;
    if (interval == 0) interval = 10;
    if (interval > 0) frameSlot = currentFrame % interval;
    keyboard->PollRequest->FrameIndex = frameSlot;

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

    keyboard->Polling = 1;
    keyboard->Configured = 1;
    return keyboard;
}

void USBKeyboardPoll(USBKeyboard *keyboard) {
    UHCITransferDescriptor *td;
    u8 *data;
    volatile u32 controlStatus;
    u32 ctrl_ls;

    if (!keyboard || !keyboard->Polling || !keyboard->PollRequest) return;
    if (keyboard->IsPolling) return;
    keyboard->IsPolling = 1;

    td = keyboard->PollRequest->TD;
    if (!td) {
        keyboard->IsPolling = 0;
        return;
    }

    controlStatus = td->ControlStatus;
    ctrl_ls = keyboard->IsLowSpeed ? UHCI_TD_SPD : 0;

    if (controlStatus & UHCI_TD_ACTIVE) {
        keyboard->IsPolling = 0;
        return;
    }

    if (controlStatus & UHCI_TD_STALL) {
        td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | ctrl_ls | ((USB_KEYBOARD_REPORT_SIZE - 1) & 0x7FF);
        td->Token &= ~(1 << 19);
        keyboard->IsPolling = 0;
        return;
    }

    if (controlStatus & (UHCI_TD_CRCERR | UHCI_TD_BITSTUFF | UHCI_TD_BABBLE | UHCI_TD_DBUFERR)) {
        td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | ctrl_ls | ((USB_KEYBOARD_REPORT_SIZE - 1) & 0x7FF);
        td->Token &= ~(1 << 19);
        keyboard->IsPolling = 0;
        return;
    }

    data = keyboard->ReportBuffer;
    keyboard->LastReport.Modifiers = data[0];
    keyboard->LastReport.Reserved = data[1];
    keyboard->LastReport.KeyCode[0] = data[2];
    keyboard->LastReport.KeyCode[1] = data[3];
    keyboard->LastReport.KeyCode[2] = data[4];
    keyboard->LastReport.KeyCode[3] = data[5];
    keyboard->LastReport.KeyCode[4] = data[6];
    keyboard->LastReport.KeyCode[5] = data[7];

    td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | ctrl_ls | ((USB_KEYBOARD_REPORT_SIZE - 1) & 0x7FF);
    td->Token ^= (1 << 19);
    keyboard->IsPolling = 0;
}

u8 USBKeyboardGetModifiers(USBKeyboard *keyboard) {
    if (!keyboard) return 0;
    return keyboard->LastReport.Modifiers;
}

u8 USBKeyboardGetKeyCount(USBKeyboard *keyboard) {
    u8 count = 0;
    u8 i;
    if (!keyboard) return 0;
    for (i = 0; i < 6; i++) {
        if (keyboard->LastReport.KeyCode[i] != KEY_NONE) count++;
    }
    return count;
}

u8 USBKeyboardGetKey(USBKeyboard *keyboard, u8 index) {
    if (!keyboard || index >= 6) return KEY_NONE;
    return keyboard->LastReport.KeyCode[index];
}

u8 USBKeyboardIsKeyPressed(USBKeyboard *keyboard, u8 keyCode) {
    u8 i;
    if (!keyboard) return 0;
    for (i = 0; i < 6; i++) {
        if (keyboard->LastReport.KeyCode[i] == keyCode) return 1;
    }
    return 0;
}

static const char* KeyMap[128] = {
    NULL, "Error", NULL, NULL,
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z",
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    "Enter", "Escape", "Backspace", "Tab", "Space",
    "-", "=", "[", "]", "\\",
    NULL, ";", "'", "`",
    ",", ".", "/",
    "CapsLock",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "PrintScreen", "ScrollLock", "Pause",
    "Insert", "Home", "PageUp", "Delete", "End", "PageDown",
    "Right", "Left", "Down", "Up",
    "NumLock", "KP/", "KP*", "KP-", "KP+", "KPEnter",
    "KP1", "KP2", "KP3", "KP4", "KP5", "KP6", "KP7", "KP8", "KP9", "KP0",
    "KP."
};

void USBKeyboardPrintKeys(USBKeyboard *keyboard) {
    u8 modifiers;
    u8 count;
    u8 key;
    u8 i;
    if (!keyboard) return;
    modifiers = USBKeyboardGetModifiers(keyboard);
    count = USBKeyboardGetKeyCount(keyboard);
    if (count == 0 && modifiers == 0) return;
    DebugStr("[");
    if (modifiers & KEY_MODIFIER_LEFTCTRL) DebugStr("LCtrl+");
    if (modifiers & KEY_MODIFIER_LEFTSHIFT) DebugStr("LShift+");
    if (modifiers & KEY_MODIFIER_LEFTALT) DebugStr("LAlt+");
    if (modifiers & KEY_MODIFIER_LEFTGUI) DebugStr("LGui+");
    if (modifiers & KEY_MODIFIER_RIGHTCTRL) DebugStr("RCtrl+");
    if (modifiers & KEY_MODIFIER_RIGHTSHIFT) DebugStr("RShift+");
    if (modifiers & KEY_MODIFIER_RIGHTALT) DebugStr("RAlt+");
    if (modifiers & KEY_MODIFIER_RIGHTGUI) DebugStr("RGui+");
    for (i = 0; i < count; i++) {
        key = USBKeyboardGetKey(keyboard, i);
        if (key < 128 && KeyMap[key] != NULL) {
            DebugStr(KeyMap[key]);
        } else {
            DebugStr("0x");
            DebugU8(key);
        }
        if (i < count - 1) DebugStr(",");
    }
    if (count == 0 && modifiers != 0) DebugStr("Modifiers");
    DebugStr("]\n");
}

void USBKeyboardTest(UHCIContext *uhci) {
    USBKeyboard *keyboard;
    int i;
    u8 prevModifiers;
    u8 prevKeys[6];
    u8 j;
    u8 modifiers;
    u8 changed;
    prevModifiers = 0;
    for (j = 0; j < 6; j++) prevKeys[j] = 0;
    keyboard = USBKeyboardInit(uhci, 0);
    if (!keyboard) {
        DebugStr("Failed to initialize keyboard\n");
        return;
    }
    for (i = 0; i < 3000; i++) {
        USBKeyboardPoll(keyboard);
        modifiers = USBKeyboardGetModifiers(keyboard);
        changed = 0;
        if (modifiers != prevModifiers) changed = 1;
        for (j = 0; j < 6; j++) {
            if (USBKeyboardGetKey(keyboard, j) != prevKeys[j]) {
                changed = 1;
                break;
            }
        }
        if (changed) {
            USBKeyboardPrintKeys(keyboard);
            prevModifiers = modifiers;
            for (j = 0; j < 6; j++) {
                prevKeys[j] = USBKeyboardGetKey(keyboard, j);
            }
        }
        SystemBusySleepMs(10);
    }
}