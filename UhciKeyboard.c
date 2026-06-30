#include "UhciKeyboard.h"
#include "debug.h"

static UHCIResult WaitForTD(UHCITransferDescriptor *td, u32 timeout_us) {
    u32 elapsed = 0;
    
    while (td->ControlStatus & UHCI_TD_ACTIVE) {
        for (u32 i = 0; i < 100; i++) {
            __asm__ volatile ("pause");
        }
        elapsed += 100;
        if (elapsed > timeout_us) {
            return UHCI_ERROR_TIMEOUT;
        }
    }
    
    if (td->ControlStatus & UHCI_TD_STALL) {
        return UHCI_ERROR_STALL;
    }
    if (td->ControlStatus & UHCI_TD_CRCERR) {
        return UHCI_ERROR_CRC;
    }
    if (td->ControlStatus & UHCI_TD_BITSTUFF) {
        return UHCI_ERROR_BITSTUFF;
    }
    if (td->ControlStatus & UHCI_TD_BABBLE) {
        return UHCI_ERROR_BABBLE;
    }
    if (td->ControlStatus & UHCI_TD_DBUFERR) {
        return UHCI_ERROR_BUFFER;
    }
    
    return UHCI_OK;
}

static UHCIResult BuildControlTransfer(UHCIContext *ctx, 
                                        u8 devAddr, u8 endpoint,
                                        u8 *setupPacket, u16 setupLen,
                                        u8 *data, u16 dataLen, u8 dir,
                                        UHCITransferDescriptor **outTD,
                                        u32 *outTDPhys) {
    AllocPool *pool = ctx->MemoryPool;
    UHCITransferDescriptor *tds;
    u32 tdsPhys;
    u32 tdCount = 3;
    
    tds = Alloc(pool, sizeof(UHCITransferDescriptor) * tdCount);
    if (!tds) return UHCI_ERROR_NO_MEMORY;
    tdsPhys = (u32)(u64)tds;
    
    MemSet(tds, 0, sizeof(UHCITransferDescriptor) * tdCount);
    
    tds[0].Token = 0;
    tds[0].Token |= UHCI_TD_PID_SETUP;
    tds[0].Token |= (devAddr << UHCI_TD_TOKEN_DEVADDR_SHIFT) & UHCI_TD_TOKEN_DEVADDR_MASK;
    tds[0].Token |= (endpoint << UHCI_TD_TOKEN_ENDPT_SHIFT) & UHCI_TD_TOKEN_ENDPT_MASK;
    tds[0].Token |= (setupLen << UHCI_TD_TOKEN_DATALEN_SHIFT) & UHCI_TD_TOKEN_DATALEN_MASK;
    
    tds[0].ControlStatus = UHCI_TD_ACTIVE | (setupLen & UHCI_TD_MAXLEN_MASK);
    
    tds[0].BufferPointer = (u32)(u64)setupPacket;
    tds[0].LinkPointer = tdsPhys + sizeof(UHCITransferDescriptor);
    
    tds[1].Token = 0;
    if (dir == 0) {
        tds[1].Token |= UHCI_TD_PID_OUT;
    } else {
        tds[1].Token |= UHCI_TD_PID_IN;
    }
    tds[1].Token |= (devAddr << UHCI_TD_TOKEN_DEVADDR_SHIFT) & UHCI_TD_TOKEN_DEVADDR_MASK;
    tds[1].Token |= (endpoint << UHCI_TD_TOKEN_ENDPT_SHIFT) & UHCI_TD_TOKEN_ENDPT_MASK;
    tds[1].Token |= (dataLen << UHCI_TD_TOKEN_DATALEN_SHIFT) & UHCI_TD_TOKEN_DATALEN_MASK;
    
    tds[1].ControlStatus = UHCI_TD_ACTIVE | (dataLen & UHCI_TD_MAXLEN_MASK);
    
    if (data && dataLen > 0) {
        tds[1].BufferPointer = (u32)(u64)data;
    } else {
        tds[1].BufferPointer = 0;
    }
    tds[1].LinkPointer = tdsPhys + sizeof(UHCITransferDescriptor) * 2;
    
    tds[2].Token = 0;
    if (dir == 0) {
        tds[2].Token |= UHCI_TD_PID_IN;
    } else {
        tds[2].Token |= UHCI_TD_PID_OUT;
    }
    tds[2].Token |= (devAddr << UHCI_TD_TOKEN_DEVADDR_SHIFT) & UHCI_TD_TOKEN_DEVADDR_MASK;
    tds[2].Token |= (endpoint << UHCI_TD_TOKEN_ENDPT_SHIFT) & UHCI_TD_TOKEN_ENDPT_MASK;
    tds[2].Token |= (0 << UHCI_TD_TOKEN_DATALEN_SHIFT);
    
    tds[2].ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC;
    tds[2].BufferPointer = 0;
    tds[2].LinkPointer = UHCI_FRAME_TERMINATE;
    
    *outTD = tds;
    *outTDPhys = tdsPhys;
    
    return UHCI_OK;
}

static UHCIResult ExecuteControlTransfer(UHCIContext *ctx,
                                          u8 devAddr, u8 endpoint,
                                          u8 *setupPacket, u16 setupLen,
                                          u8 *data, u16 dataLen, u8 dir) {
    UHCITransferDescriptor *tds;
    u32 tdsPhys;
    UHCIResult result;
    
    result = BuildControlTransfer(ctx, devAddr, endpoint, 
                                   setupPacket, setupLen,
                                   data, dataLen, dir,
                                   &tds, &tdsPhys);
    if (result != UHCI_OK) return result;
    
    UHCIFrameListEntry *frameList = (UHCIFrameListEntry*)ctx->FrameListVirt;
    frameList[0].Pointer = tdsPhys;
    
    result = WaitForTD(&tds[2], USB_CTRL_TIMEOUT);
    
    frameList[0].Pointer = UHCI_FRAME_TERMINATE;
    
    Free(ctx->MemoryPool, tds);
    
    return result;
}

static void BuildSetupPacket(u8 *packet, u8 bmRequestType, u8 bRequest,
                              u16 wValue, u16 wIndex, u16 wLength) {
    packet[0] = bmRequestType;
    packet[1] = bRequest;
    packet[2] = wValue & 0xFF;
    packet[3] = (wValue >> 8) & 0xFF;
    packet[4] = wIndex & 0xFF;
    packet[5] = (wIndex >> 8) & 0xFF;
    packet[6] = wLength & 0xFF;
    packet[7] = (wLength >> 8) & 0xFF;
}

static UHCIResult GetDeviceDescriptor(UHCIContext *ctx, u8 devAddr,
                                       USBDeviceDescriptor *desc) {
    u8 setup[8];
    u8 data[18];
    
    BuildSetupPacket(setup, 
                     0x80,
                     USB_REQ_GET_DESCRIPTOR,
                     0x0100,
                     0x0000,
                     0x0012);
    
    UHCIResult result = ExecuteControlTransfer(ctx, devAddr, 0,
                                                setup, 8,
                                                data, 18, 1);
    
    if (result != UHCI_OK) return result;
    
    MemCopy(desc, data, sizeof(USBDeviceDescriptor));
    return UHCI_OK;
}

static UHCIResult SetDeviceAddress(UHCIContext *ctx, u8 devAddr) {
    u8 setup[8];
    
    BuildSetupPacket(setup,
                     0x00,
                     0x05,
                     devAddr,
                     0x0000,
                     0x0000);
    
    return ExecuteControlTransfer(ctx, 0, 0, setup, 8, NULL, 0, 0);
}

static UHCIResult GetConfigDescriptor(UHCIContext *ctx, u8 devAddr,
                                       USBConfigDescriptor *desc) {
    u8 setup[8];
    u8 data[9];
    
    BuildSetupPacket(setup,
                     0x80,
                     USB_REQ_GET_DESCRIPTOR,
                     0x0200,
                     0x0000,
                     0x0009);
    
    return ExecuteControlTransfer(ctx, devAddr, 0,
                                   setup, 8,
                                   data, 9, 1);
}

static UHCIResult SetConfiguration(UHCIContext *ctx, u8 devAddr, u8 configValue) {
    u8 setup[8];
    
    BuildSetupPacket(setup,
                     0x00,
                     USB_REQ_SET_CONFIG,
                     configValue,
                     0x0000,
                     0x0000);
    
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0);
}

static UHCIResult SetIdle(UHCIContext *ctx, u8 devAddr, u8 duration) {
    u8 setup[8];
    
    BuildSetupPacket(setup,
                     0x21,
                     USB_REQ_SET_IDLE,
                     (duration << 8),
                     0x0000,
                     0x0000);
    
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0);
}

static UHCIResult SetProtocol(UHCIContext *ctx, u8 devAddr, u8 protocol) {
    u8 setup[8];
    
    BuildSetupPacket(setup,
                     0x21,
                     USB_REQ_SET_PROTOCOL,
                     protocol,
                     0x0000,
                     0x0000);
    
    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0);
}

static UHCIResult CreateInterruptIN(UHCIContext *ctx, u8 devAddr, u8 endpoint,
                                     u8 *buffer, u16 len,
                                     UHCITransferDescriptor **outTD,
                                     u32 *outTDPhys) {
    AllocPool *pool = ctx->MemoryPool;
    UHCITransferDescriptor *td;
    u32 tdPhys;
    
    td = Alloc(pool, sizeof(UHCITransferDescriptor));
    if (!td) return UHCI_ERROR_NO_MEMORY;
    tdPhys = (u32)(u64)td;
    
    MemSet(td, 0, sizeof(UHCITransferDescriptor));
    
    td->Token = 0;
    td->Token |= UHCI_TD_PID_IN;
    td->Token |= (devAddr << UHCI_TD_TOKEN_DEVADDR_SHIFT) & UHCI_TD_TOKEN_DEVADDR_MASK;
    td->Token |= (endpoint << UHCI_TD_TOKEN_ENDPT_SHIFT) & UHCI_TD_TOKEN_ENDPT_MASK;
    td->Token |= (len << UHCI_TD_TOKEN_DATALEN_SHIFT) & UHCI_TD_TOKEN_DATALEN_MASK;
    
    td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | (len & UHCI_TD_MAXLEN_MASK);
    
    td->BufferPointer = (u32)(u64)buffer;
    td->LinkPointer = tdPhys;
    
    *outTD = td;
    *outTDPhys = tdPhys;
    
    return UHCI_OK;
}

USBKeyboard* USBKeyboardInit(UHCIContext *uhci, u8 port) {
    USBKeyboard *keyboard;
    USBDeviceDescriptor devDesc;
    USBConfigDescriptor configDesc;
    u8 setup[8];
    UHCIResult result;
    u16 currentFrame;
    
    if (!uhci || !uhci->MemoryPool) return NULL;
    
    keyboard = Alloc(uhci->MemoryPool, sizeof(USBKeyboard));
    if (!keyboard) return NULL;
    MemSet(keyboard, 0, sizeof(USBKeyboard));
    keyboard->UHCI = uhci;
    
    DebugStr("USBKeyboardInit: Starting keyboard initialization on port ");
    DebugU64(port);
    DebugChar('\n');
    
    DebugStr("Resetting port...\n");
    result = UHCIResetPort(uhci, port);
    if (result != UHCI_OK) {
        DebugStr("Port reset failed\n");
        return NULL;
    }
    
    SystemBusySleepMs(100);
    
    DebugStr("Getting device descriptor (address 0)...\n");
    result = GetDeviceDescriptor(uhci, 0, &devDesc);
    if (result != UHCI_OK) {
        DebugStr("Get device descriptor failed\n");
        return NULL;
    }
    
    DebugStr("  Vendor: 0x");
    DebugU64(devDesc.idVendor);
    DebugStr(" Product: 0x");
    DebugU64(devDesc.idProduct);
    DebugChar('\n');
    
    if (devDesc.bDeviceClass != 0x00 && devDesc.bDeviceClass != 0x03) {
        DebugStr("Not a HID device\n");
        return NULL;
    }
    
    keyboard->DeviceAddress = 1;
    DebugStr("Setting device address to ");
    DebugU64(keyboard->DeviceAddress);
    DebugChar('\n');
    
    result = SetDeviceAddress(uhci, keyboard->DeviceAddress);
    if (result != UHCI_OK) {
        DebugStr("Set address failed\n");
        return NULL;
    }
    SystemBusySleepMs(10);
    
    DebugStr("Getting configuration descriptor...\n");
    result = GetConfigDescriptor(uhci, keyboard->DeviceAddress, &configDesc);
    if (result != UHCI_OK) {
        DebugStr("Get config descriptor failed\n");
        return NULL;
    }
    
    DebugStr("  Config value: ");
    DebugU64(configDesc.bConfigurationValue);
    DebugStr("  bNumInterfaces: ");
    DebugU64(configDesc.bNumInterfaces);
    DebugChar('\n');
    
    DebugStr("Setting configuration...\n");
    result = SetConfiguration(uhci, keyboard->DeviceAddress, 
                               configDesc.bConfigurationValue);
    if (result != UHCI_OK) {
        DebugStr("Set configuration failed\n");
        return NULL;
    }
    SystemBusySleepMs(10);
    
    DebugStr("Setting Boot protocol...\n");
    result = SetProtocol(uhci, keyboard->DeviceAddress, USB_KEYBOARD_PROTOCOL_BOOT);
    if (result != UHCI_OK) {
        DebugStr("Set protocol failed (may not be supported), trying Report mode...\n");
        result = SetProtocol(uhci, keyboard->DeviceAddress, USB_KEYBOARD_PROTOCOL_REPORT);
        if (result != UHCI_OK) {
            DebugStr("Set protocol failed\n");
            return NULL;
        }
    }
    
    DebugStr("Setting idle...\n");
    result = SetIdle(uhci, keyboard->DeviceAddress, 0);
    if (result != UHCI_OK) {
        DebugStr("Set idle failed (optional, ignoring)\n");
    }
    
    DebugStr("Getting full configuration descriptor...\n");
    u8 configData[256];
    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR,
                     0x0200, 0x0000, 255);
    result = ExecuteControlTransfer(uhci, keyboard->DeviceAddress, 0,
                                     setup, 8,
                                     configData, 255, 1);
    if (result != UHCI_OK) {
        DebugStr("Get full config descriptor failed\n");
        return NULL;
    }
    
    u8 *ptr = configData;
    u8 *end = ptr + ((USBConfigDescriptor*)ptr)->wTotalLength;
    u8 foundEndpoint = 0;
    
    while (ptr < end) {
        u8 len = ptr[0];
        u8 type = ptr[1];
        
        if (type == 0x05) {
            USBEndpointDescriptor *ep = (USBEndpointDescriptor*)ptr;
            if ((ep->bEndpointAddress & 0x80) && 
                (ep->bmAttributes & 0x03) == 0x03) {
                keyboard->InterruptEndpoint = ep->bEndpointAddress & 0x0F;
                keyboard->MaxPacketSize = ep->wMaxPacketSize;
                keyboard->InterruptInterval = ep->bInterval;
                foundEndpoint = 1;
                DebugStr("Found interrupt endpoint: ");
                DebugU64(keyboard->InterruptEndpoint);
                DebugStr(" MaxPacket: ");
                DebugU64(keyboard->MaxPacketSize);
                DebugStr(" Interval: ");
                DebugU64(keyboard->InterruptInterval);
                DebugChar('\n');
                break;
            }
        }
        ptr += len;
    }
    
    if (!foundEndpoint) {
        DebugStr("No interrupt endpoint found!\n");
        return NULL;
    }
    
    DebugStr("Creating interrupt IN TD...\n");
    
    keyboard->PollRequest = UHCICreateRequest(uhci);
    if (!keyboard->PollRequest) {
        DebugStr("Create request failed\n");
        return NULL;
    }
    
    UHCITransferDescriptor *td;
    u32 tdPhys;
    result = CreateInterruptIN(uhci, keyboard->DeviceAddress,
                                keyboard->InterruptEndpoint,
                                keyboard->ReportBuffer,
                                USB_KEYBOARD_REPORT_SIZE,
                                &td, &tdPhys);
    if (result != UHCI_OK) {
        DebugStr("Create interrupt TD failed\n");
        return NULL;
    }
    
    keyboard->PollRequest->DeviceAddress = keyboard->DeviceAddress;
    keyboard->PollRequest->Endpoint = keyboard->InterruptEndpoint;
    keyboard->PollRequest->Type = UHCI_TRANSFER_INTERRUPT;
    keyboard->PollRequest->DataBuffer = keyboard->ReportBuffer;
    keyboard->PollRequest->DataLength = USB_KEYBOARD_REPORT_SIZE;
    keyboard->PollRequest->MaxPacketSize = keyboard->MaxPacketSize;
    keyboard->PollRequest->TD = td;
    
    currentFrame = UHCIGetFrameNumber(uhci);
    u32 frameSlot = 0;
    if (keyboard->InterruptInterval > 0) {
        frameSlot = currentFrame % keyboard->InterruptInterval;
    }
    
    UHCIFrameListEntry *frameList = (UHCIFrameListEntry*)uhci->FrameListVirt;
    frameList[frameSlot].Pointer = tdPhys;
    
    keyboard->Polling = 1;
    keyboard->Configured = 1;
    
    DebugStr("USB keyboard initialized successfully!\n");
    DebugStr("  Address: ");
    DebugU64(keyboard->DeviceAddress);
    DebugStr("  Endpoint: ");
    DebugU64(keyboard->InterruptEndpoint);
    DebugStr("  Report size: 8 bytes\n");
    DebugStr("  Frame slot: ");
    DebugU64(frameSlot);
    DebugChar('\n');
    
    return keyboard;
}

void USBKeyboardPoll(USBKeyboard *keyboard) {
    if (!keyboard || !keyboard->Polling) return;
    
    UHCITransferDescriptor *td = keyboard->PollRequest->TD;
    
    if (!(td->ControlStatus & UHCI_TD_ACTIVE)) {
        if (td->ControlStatus & UHCI_TD_STALL) {
            DebugStr("Keyboard stall error\n");
            td->ControlStatus = UHCI_TD_ACTIVE | USB_KEYBOARD_REPORT_SIZE;
            return;
        }
        
        u8 *data = keyboard->ReportBuffer;
        
        keyboard->LastReport.Modifiers = data[0];
        keyboard->LastReport.Reserved = data[1];
        keyboard->LastReport.KeyCode[0] = data[2];
        keyboard->LastReport.KeyCode[1] = data[3];
        keyboard->LastReport.KeyCode[2] = data[4];
        keyboard->LastReport.KeyCode[3] = data[5];
        keyboard->LastReport.KeyCode[4] = data[6];
        keyboard->LastReport.KeyCode[5] = data[7];
        
        td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | USB_KEYBOARD_REPORT_SIZE;
    }
}

u8 USBKeyboardGetModifiers(USBKeyboard *keyboard) {
    if (!keyboard) return 0;
    return keyboard->LastReport.Modifiers;
}

u8 USBKeyboardGetKeyCount(USBKeyboard *keyboard) {
    u8 count = 0;
    if (!keyboard) return 0;
    
    for (u8 i = 0; i < 6; i++) {
        if (keyboard->LastReport.KeyCode[i] != KEY_NONE) {
            count++;
        }
    }
    return count;
}

u8 USBKeyboardGetKey(USBKeyboard *keyboard, u8 index) {
    if (!keyboard || index >= 6) return KEY_NONE;
    return keyboard->LastReport.KeyCode[index];
}

u8 USBKeyboardIsKeyPressed(USBKeyboard *keyboard, u8 keyCode) {
    if (!keyboard) return 0;
    
    for (u8 i = 0; i < 6; i++) {
        if (keyboard->LastReport.KeyCode[i] == keyCode) {
            return 1;
        }
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
    
    for (u8 i = 0; i < count; i++) {
        key = USBKeyboardGetKey(keyboard, i);
        if (key < 128 && KeyMap[key] != NULL) {
            DebugStr(KeyMap[key]);
        } else {
            DebugStr("0x");
            DebugU8(key);
        }
        if (i < count - 1) DebugStr(",");
    }
    
    if (count == 0 && modifiers != 0) {
        DebugStr("Modifiers");
    }
    
    DebugStr("]\n");
}

void USBKeyboardTest(UHCIContext *uhci) {
    USBKeyboard *keyboard;
    
    DebugStr("\n=== USB Keyboard Test ===\n");
    
    keyboard = USBKeyboardInit(uhci, 0);
    if (!keyboard) {
        DebugStr("Failed to initialize keyboard\n");
        return;
    }
    
    DebugStr("Keyboard ready! Polling for 30 seconds...\n");
    DebugStr("Press keys to see output\n\n");
    
    u8 prevModifiers = 0;
    u8 prevKeys[6] = {0};
    
    for (int i = 0; i < 3000; i++) {
        USBKeyboardPoll(keyboard);
        
        u8 modifiers = USBKeyboardGetModifiers(keyboard);
        u8 count = USBKeyboardGetKeyCount(keyboard);
        
        u8 changed = 0;
        if (modifiers != prevModifiers) changed = 1;
        for (u8 j = 0; j < 6; j++) {
            if (USBKeyboardGetKey(keyboard, j) != prevKeys[j]) {
                changed = 1;
                break;
            }
        }
        
        if (changed) {
            USBKeyboardPrintKeys(keyboard);
            prevModifiers = modifiers;
            for (u8 j = 0; j < 6; j++) {
                prevKeys[j] = USBKeyboardGetKey(keyboard, j);
            }
        }
        
        SystemBusySleepMs(10);
    }
    
    DebugStr("\nKeyboard test complete\n");
}