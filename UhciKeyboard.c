#include "UhciKeyboard.h"
#include "debug.h"
#include "uhci.h"

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
    DebugStr("USBKeyboardInit start on port ");
    DebugU8(port);
    DebugStr("\n");

    USBKeyboard *keyboard;
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
    u8 foundKbdInterface;
    u16 port_status;
    u8 is_low_speed;
    u32 interval;
    u8 assignedAddr;
    u8 maxPacketSize0;
    u8 configValue;
    if (!uhci || !uhci->MemoryPool) return NULL;
    keyboard = (USBKeyboard*)MaxAlloc(uhci->MemoryPool, sizeof(USBKeyboard), UHCI_MAX_ADDRESS);
    if (!keyboard) return NULL;
    MemSet(keyboard, 0, sizeof(USBKeyboard));
    keyboard->UHCI = uhci;
    SystemBusySleepMs(100);
    port_status = UHCIGetPortStatus(uhci, port);
    is_low_speed = (port_status & UHCI_PORTSC_LSDA) ? 1 : 0;
    DebugStr("Port "); DebugU8(port);
    DebugStr(" LowSpeed = "); DebugU8(is_low_speed);
    DebugStr(" (status=0x"); DebugU16(port_status); DebugStr(")\n");
    keyboard->IsLowSpeed = is_low_speed;
    result = GetDeviceDescriptor(uhci, 0, &devDesc, is_low_speed);
    if (result != UHCI_OK) return NULL;
    maxPacketSize0 = devDesc.bMaxPacketSize0;
    keyboard->MaxPacketSize0 = maxPacketSize0;
    assignedAddr = 1;
    keyboard->DeviceAddress = assignedAddr;
    result = SetDeviceAddress(uhci, assignedAddr, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) return NULL;
    SystemBusySleepMs(10);
    {
        u8 configData[256] __attribute__((aligned(16)));
        u16 totalLength;
        BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, 255);
        result = ExecuteControlTransfer(uhci, keyboard->DeviceAddress, 0, setup, 8, configData, 255, 1, is_low_speed, maxPacketSize0);
        if (result != UHCI_OK) return NULL;
        totalLength = configData[2] | (configData[3] << 8);
        if (totalLength > 255) totalLength = 255;
        MemSet(configData, 0, 256);
        BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR, 0x0200, 0x0000, totalLength);
        result = ExecuteControlTransfer(uhci, keyboard->DeviceAddress, 0, setup, 8, configData, totalLength, 1, is_low_speed, maxPacketSize0);
        if (result != UHCI_OK) return NULL;
        configValue = configData[5];
        ptr = configData;
        end = ptr + totalLength;
        foundEndpoint = 0;
        foundKbdInterface = 0;
        while (ptr < end) {
            u8 len = ptr[0];
            u8 type = ptr[1];
            if (type == 0x04) {
                u8 intfClass    = ptr[5];
                u8 intfSubClass = ptr[6];
                u8 intfProtocol = ptr[7];

                if (intfClass == 0x03 && intfSubClass == 0x01 && intfProtocol == 0x01) {
                    foundKbdInterface = 1;
                } else {
                    foundKbdInterface = 0;
                }
            }
            if (type == 0x05 && foundKbdInterface) {
                if ((ptr[2] & 0x80) && (ptr[3] & 0x03) == 0x03) {
                    keyboard->InterruptEndpoint = ptr[2] & 0x0F;
                    keyboard->MaxPacketSize = ptr[4] | (ptr[5] << 8);
                    keyboard->InterruptInterval = ptr[6];
                    foundEndpoint = 1;
                    break;
                }
            }
            ptr += len;
            if (len == 0) break;
        }
    }
    if (!foundEndpoint) return NULL;
    result = SetConfiguration(uhci, keyboard->DeviceAddress, configValue, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) return NULL;
    SystemBusySleepMs(10);
    result = SetProtocol(uhci, keyboard->DeviceAddress, USB_KEYBOARD_PROTOCOL_BOOT, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) {
        result = SetProtocol(uhci, keyboard->DeviceAddress, USB_KEYBOARD_PROTOCOL_REPORT, is_low_speed, maxPacketSize0);
        if (result != UHCI_OK) return NULL;
    }
    result = SetIdle(uhci, keyboard->DeviceAddress, 0, is_low_speed, maxPacketSize0);
    if (result != UHCI_OK) {}
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
    interval = keyboard->InterruptInterval;
    if (interval == 0) interval = 10;
    frameSlot = currentFrame % interval;
    keyboard->PollRequest->FrameIndex = frameSlot;

    frameList = uhci->FrameList;
    if (frameList) {
        for (u32 i = frameSlot; i < 1024; i += interval) {
            frameList[i].Pointer = ((u32)(u64)qh & 0xFFFFFFF0) | UHCI_FRAME_QH;
        }
    } else {
        goto cleanup;
    }

    UHCISubmitRequest(uhci, keyboard->PollRequest);

    keyboard->Polling = 1;
    keyboard->Configured = 1;
    return keyboard;
cleanup:
    if (keyboard) {
        if (keyboard->PollRequest) {
            if (keyboard->PollRequest->QH) {
                UHCIClearFrameListEntry(uhci, keyboard->PollRequest->QH);
                Free(uhci->MemoryPool, keyboard->PollRequest->QH);
            }
            if (keyboard->PollRequest->TD) {
                Free(uhci->MemoryPool, keyboard->PollRequest->TD);
            }
            Free(uhci->MemoryPool, keyboard->PollRequest);
        }
        Free(uhci->MemoryPool, keyboard);
    }
    if (uhci) {
        UHCIResetPort(uhci, port);
    }
    return NULL;
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
    ctrl_ls = keyboard->IsLowSpeed ? UHCI_TD_LS : 0;
    if (controlStatus & UHCI_TD_ACTIVE) {
        keyboard->IsPolling = 0;
        return;
    }
    if (controlStatus & UHCI_TD_STALL) {
        td->ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((USB_KEYBOARD_REPORT_SIZE - 1) & 0x7FF);
        td->Token &= ~(1 << 19);
        keyboard->IsPolling = 0;
        return;
    }
    if (controlStatus & (UHCI_TD_CRCERR | UHCI_TD_BITSTUFF | UHCI_TD_BABBLE | UHCI_TD_DBUFERR)) {
        td->ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((USB_KEYBOARD_REPORT_SIZE - 1) & 0x7FF);
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
    td->ControlStatus = UHCI_TD_ACTIVE | ctrl_ls | ((USB_KEYBOARD_REPORT_SIZE - 1) & 0x7FF);
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