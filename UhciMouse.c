#include "UhciMouse.h"
#include "debug.h"
#include "clock.h"

static UHCIResult WaitForTD(UHCITransferDescriptor *td, u32 timeout_us)
{
    u32 elapsed = 0;

    while (td->ControlStatus & UHCI_TD_ACTIVE)
    {
        for (u32 i = 0; i < 100; i++)
        {
            __asm__ volatile("pause");
        }
        elapsed += 100;
        if (elapsed > timeout_us)
        {
            return UHCI_ERROR_TIMEOUT;
        }
    }

    if (td->ControlStatus & UHCI_TD_STALL)
    {
        return UHCI_ERROR_STALL;
    }
    if (td->ControlStatus & UHCI_TD_CRCERR)
    {
        return UHCI_ERROR_CRC;
    }
    if (td->ControlStatus & UHCI_TD_BITSTUFF)
    {
        return UHCI_ERROR_BITSTUFF;
    }
    if (td->ControlStatus & UHCI_TD_BABBLE)
    {
        return UHCI_ERROR_BABBLE;
    }
    if (td->ControlStatus & UHCI_TD_DBUFERR)
    {
        return UHCI_ERROR_BUFFER;
    }

    return UHCI_OK;
}

static UHCIResult BuildControlTransfer(UHCIContext *ctx,
                                       u8 devAddr, u8 endpoint,
                                       u8 *setupPacket, u16 setupLen,
                                       u8 *data, u16 dataLen, u8 dir,
                                       UHCITransferDescriptor **outTD,
                                       u32 *outTDPhys)
{
    AllocPool *pool = ctx->MemoryPool;
    UHCITransferDescriptor *tds;
    u32 tdsPhys;
    u32 tdCount = 3;

    tds = Alloc(pool, sizeof(UHCITransferDescriptor) * tdCount);
    if (!tds)
        return UHCI_ERROR_NO_MEMORY;
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
    if (dir == 0)
    {
        tds[1].Token |= UHCI_TD_PID_OUT;
    }
    else
    {
        tds[1].Token |= UHCI_TD_PID_IN;
    }
    tds[1].Token |= (devAddr << UHCI_TD_TOKEN_DEVADDR_SHIFT) & UHCI_TD_TOKEN_DEVADDR_MASK;
    tds[1].Token |= (endpoint << UHCI_TD_TOKEN_ENDPT_SHIFT) & UHCI_TD_TOKEN_ENDPT_MASK;
    tds[1].Token |= (dataLen << UHCI_TD_TOKEN_DATALEN_SHIFT) & UHCI_TD_TOKEN_DATALEN_MASK;

    tds[1].ControlStatus = UHCI_TD_ACTIVE | (dataLen & UHCI_TD_MAXLEN_MASK);

    if (data && dataLen > 0)
    {
        tds[1].BufferPointer = (u32)(u64)data;
    }
    else
    {
        tds[1].BufferPointer = 0;
    }
    tds[1].LinkPointer = tdsPhys + sizeof(UHCITransferDescriptor) * 2;

    tds[2].Token = 0;
    if (dir == 0)
    {
        tds[2].Token |= UHCI_TD_PID_IN;
    }
    else
    {
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
                                         u8 *data, u16 dataLen, u8 dir)
{
    UHCITransferDescriptor *tds;
    u32 tdsPhys;
    UHCIResult result;

    result = BuildControlTransfer(ctx, devAddr, endpoint,
                                  setupPacket, setupLen,
                                  data, dataLen, dir,
                                  &tds, &tdsPhys);
    if (result != UHCI_OK)
        return result;

    UHCIFrameListEntry *frameList = (UHCIFrameListEntry *)ctx->FrameListVirt;
    frameList[0].Pointer = tdsPhys;

    result = WaitForTD(&tds[2], USB_CTRL_TIMEOUT);

    frameList[0].Pointer = UHCI_FRAME_TERMINATE;

    Free(ctx->MemoryPool, tds);

    return result;
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

static UHCIResult GetDeviceDescriptor(UHCIContext *ctx, u8 devAddr,
                                      USBDeviceDescriptor *desc)
{
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

    if (result != UHCI_OK)
        return result;

    MemCopy(desc, data, sizeof(USBDeviceDescriptor));
    return UHCI_OK;
}

static UHCIResult SetDeviceAddress(UHCIContext *ctx, u8 devAddr)
{
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
                                      USBConfigDescriptor *desc)
{
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

static UHCIResult SetConfiguration(UHCIContext *ctx, u8 devAddr, u8 configValue)
{
    u8 setup[8];

    BuildSetupPacket(setup,
                     0x00,
                     USB_REQ_SET_CONFIG,
                     configValue,
                     0x0000,
                     0x0000);

    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0);
}

static UHCIResult SetIdle(UHCIContext *ctx, u8 devAddr, u8 duration)
{
    u8 setup[8];

    BuildSetupPacket(setup,
                     0x21,
                     USB_REQ_SET_IDLE,
                     (duration << 8),
                     0x0000,
                     0x0000);

    return ExecuteControlTransfer(ctx, devAddr, 0, setup, 8, NULL, 0, 0);
}

static UHCIResult SetProtocol(UHCIContext *ctx, u8 devAddr, u8 protocol)
{
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
                                    u32 *outTDPhys)
{
    AllocPool *pool = ctx->MemoryPool;
    UHCITransferDescriptor *td;
    u32 tdPhys;

    td = Alloc(pool, sizeof(UHCITransferDescriptor));
    if (!td)
        return UHCI_ERROR_NO_MEMORY;
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

USBMouse *USBMouseInit(UHCIContext *uhci, u8 port)
{
    USBMouse *mouse;
    USBDeviceDescriptor devDesc;
    USBConfigDescriptor configDesc;
    u8 setup[8];
    UHCIResult result;
    u16 currentFrame;

    if (!uhci || !uhci->MemoryPool)
        return NULL;

    mouse = Alloc(uhci->MemoryPool, sizeof(USBMouse));
    if (!mouse)
        return NULL;
    MemSet(mouse, 0, sizeof(USBMouse));
    mouse->UHCI = uhci;

    DebugStr("USBMouseInit: Starting mouse initialization on port ");
    DebugU64(port);
    DebugChar('\n');

    DebugStr("Resetting port...\n");
    result = UHCIResetPort(uhci, port);
    if (result != UHCI_OK)
    {
        DebugStr("Port reset failed\n");
        return NULL;
    }

    SystemBusySleepMs(100);

    DebugStr("Getting device descriptor (address 0)...\n");
    result = GetDeviceDescriptor(uhci, 0, &devDesc);
    if (result != UHCI_OK)
    {
        DebugStr("Get device descriptor failed\n");
        return NULL;
    }

    DebugStr("  Vendor: 0x");
    DebugU64(devDesc.idVendor);
    DebugStr(" Product: 0x");
    DebugU64(devDesc.idProduct);
    DebugChar('\n');

    if (devDesc.bDeviceClass != 0x00 && devDesc.bDeviceClass != 0x03)
    {
        DebugStr("Not a HID device\n");
        return NULL;
    }

    mouse->DeviceAddress = 1;
    DebugStr("Setting device address to ");
    DebugU64(mouse->DeviceAddress);
    DebugChar('\n');

    result = SetDeviceAddress(uhci, mouse->DeviceAddress);
    if (result != UHCI_OK)
    {
        DebugStr("Set address failed\n");
        return NULL;
    }
    SystemBusySleepMs(10);

    DebugStr("Getting configuration descriptor...\n");
    result = GetConfigDescriptor(uhci, mouse->DeviceAddress, &configDesc);
    if (result != UHCI_OK)
    {
        DebugStr("Get config descriptor failed\n");
        return NULL;
    }

    DebugStr("  Config value: ");
    DebugU64(configDesc.bConfigurationValue);
    DebugStr("  bNumInterfaces: ");
    DebugU64(configDesc.bNumInterfaces);
    DebugChar('\n');

    DebugStr("Setting configuration...\n");
    result = SetConfiguration(uhci, mouse->DeviceAddress,
                              configDesc.bConfigurationValue);
    if (result != UHCI_OK)
    {
        DebugStr("Set configuration failed\n");
        return NULL;
    }
    SystemBusySleepMs(10);

    DebugStr("Setting Boot protocol...\n");
    result = SetProtocol(uhci, mouse->DeviceAddress, USB_MOUSE_PROTOCOL_BOOT);
    if (result != UHCI_OK)
    {
        DebugStr("Set protocol failed (may not be supported), trying Report mode...\n");
        result = SetProtocol(uhci, mouse->DeviceAddress, USB_MOUSE_PROTOCOL_REPORT);
        if (result != UHCI_OK)
        {
            DebugStr("Set protocol failed\n");
            return NULL;
        }
    }

    DebugStr("Setting idle...\n");
    result = SetIdle(uhci, mouse->DeviceAddress, 0);
    if (result != UHCI_OK)
    {
        DebugStr("Set idle failed (optional, ignoring)\n");
    }

    DebugStr("Getting full configuration descriptor...\n");
    u8 configData[256];
    BuildSetupPacket(setup, 0x80, USB_REQ_GET_DESCRIPTOR,
                     0x0200, 0x0000, 255);
    result = ExecuteControlTransfer(uhci, mouse->DeviceAddress, 0,
                                    setup, 8,
                                    configData, 255, 1);
    if (result != UHCI_OK)
    {
        DebugStr("Get full config descriptor failed\n");
        return NULL;
    }

    u8 *ptr = configData;
    u8 *end = ptr + ((USBConfigDescriptor *)ptr)->wTotalLength;
    u8 foundEndpoint = 0;

    while (ptr < end)
    {
        u8 len = ptr[0];
        u8 type = ptr[1];

        if (type == 0x05)
        {
            USBEndpointDescriptor *ep = (USBEndpointDescriptor *)ptr;
            if ((ep->bEndpointAddress & 0x80) &&
                (ep->bmAttributes & 0x03) == 0x03)
            {
                mouse->InterruptEndpoint = ep->bEndpointAddress & 0x0F;
                mouse->MaxPacketSize = ep->wMaxPacketSize;
                mouse->InterruptInterval = ep->bInterval;
                foundEndpoint = 1;
                DebugStr("Found interrupt endpoint: ");
                DebugU64(mouse->InterruptEndpoint);
                DebugStr(" MaxPacket: ");
                DebugU64(mouse->MaxPacketSize);
                DebugStr(" Interval: ");
                DebugU64(mouse->InterruptInterval);
                DebugChar('\n');
                break;
            }
        }
        ptr += len;
    }

    if (!foundEndpoint)
    {
        DebugStr("No interrupt endpoint found!\n");
        return NULL;
    }

    DebugStr("Creating interrupt IN TD...\n");

    mouse->PollRequest = UHCICreateRequest(uhci);
    if (!mouse->PollRequest)
    {
        DebugStr("Create request failed\n");
        return NULL;
    }

    UHCITransferDescriptor *td;
    u32 tdPhys;
    result = CreateInterruptIN(uhci, mouse->DeviceAddress,
                               mouse->InterruptEndpoint,
                               mouse->ReportBuffer,
                               USB_MOUSE_REPORT_SIZE,
                               &td, &tdPhys);
    if (result != UHCI_OK)
    {
        DebugStr("Create interrupt TD failed\n");
        return NULL;
    }

    mouse->PollRequest->DeviceAddress = mouse->DeviceAddress;
    mouse->PollRequest->Endpoint = mouse->InterruptEndpoint;
    mouse->PollRequest->Type = UHCI_TRANSFER_INTERRUPT;
    mouse->PollRequest->DataBuffer = mouse->ReportBuffer;
    mouse->PollRequest->DataLength = USB_MOUSE_REPORT_SIZE;
    mouse->PollRequest->MaxPacketSize = mouse->MaxPacketSize;
    mouse->PollRequest->TD = td;

    currentFrame = UHCIGetFrameNumber(uhci);
    u32 frameSlot = 0;
    if (mouse->InterruptInterval > 0)
    {
        frameSlot = currentFrame % mouse->InterruptInterval;
    }

    UHCIFrameListEntry *frameList = (UHCIFrameListEntry *)uhci->FrameListVirt;
    frameList[frameSlot].Pointer = tdPhys;

    mouse->Polling = 1;
    mouse->Configured = 1;

    DebugStr("USB mouse initialized successfully!\n");
    DebugStr("  Address: ");
    DebugU64(mouse->DeviceAddress);
    DebugStr("  Endpoint: ");
    DebugU64(mouse->InterruptEndpoint);
    DebugStr("  Report size: 4 bytes\n");
    DebugStr("  Frame slot: ");
    DebugU64(frameSlot);
    DebugChar('\n');

    return mouse;
}

void USBMousePoll(USBMouse *mouse)
{
    if (!mouse || !mouse->Polling)
        return;

    UHCITransferDescriptor *td = mouse->PollRequest->TD;

    if (!(td->ControlStatus & UHCI_TD_ACTIVE))
    {
        if (td->ControlStatus & UHCI_TD_STALL)
        {
            DebugStr("Mouse stall error\n");
            td->ControlStatus = UHCI_TD_ACTIVE | USB_MOUSE_REPORT_SIZE;
            return;
        }

        u8 *data = mouse->ReportBuffer;

        mouse->LastReport.Buttons = data[0] & 0x07;
        mouse->LastReport.X = (s8)data[1];
        mouse->LastReport.Y = (s8)data[2];
        mouse->LastReport.Wheel = (s8)data[3];

        td->ControlStatus = UHCI_TD_ACTIVE | UHCI_TD_IOC | USB_MOUSE_REPORT_SIZE;
    }
}

u8 USBMouseGetButtons(USBMouse *mouse)
{
    if (!mouse)
        return 0;
    return mouse->LastReport.Buttons;
}

s8 USBMouseGetX(USBMouse *mouse)
{
    if (!mouse)
        return 0;
    return mouse->LastReport.X;
}

s8 USBMouseGetY(USBMouse *mouse)
{
    if (!mouse)
        return 0;
    return mouse->LastReport.Y;
}

s8 USBMouseGetWheel(USBMouse *mouse)
{
    if (!mouse)
        return 0;
    return mouse->LastReport.Wheel;
}

void USBMouseTest(UHCIContext *uhci)
{
    USBMouse *mouse;

    DebugStr("\n=== USB Mouse Test ===\n");

    mouse = USBMouseInit(uhci, 0);
    if (!mouse)
    {
        DebugStr("Failed to initialize mouse\n");
        return;
    }

    DebugStr("Mouse ready! Polling for 10 seconds...\n");
    DebugStr("Move mouse and click buttons\n\n");

    for (int i = 0; i < 1000; i++)
    {
        USBMousePoll(mouse);

        u8 buttons = USBMouseGetButtons(mouse);
        s8 x = USBMouseGetX(mouse);
        s8 y = USBMouseGetY(mouse);
        s8 wheel = USBMouseGetWheel(mouse);

        if (buttons != 0 || x != 0 || y != 0 || wheel != 0)
        {
            DebugStr("Mouse: Buttons=");
            DebugU64(buttons);
            DebugStr(" X=");
            DebugI64(x);
            DebugStr(" Y=");
            DebugI64(y);
            DebugStr(" Wheel=");
            DebugI64(wheel);
            DebugChar('\n');
        }

        SystemBusySleepMs(10);
    }

    DebugStr("\nMouse test complete\n");
}