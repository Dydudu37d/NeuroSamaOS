#pragma once
#include "uhci.h"

#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_CONFIG 0x09
#define USB_REQ_SET_IDLE 0x0A
#define USB_REQ_SET_PROTOCOL 0x0B
#define USB_MOUSE_REPORT_SIZE 4
#define USB_MOUSE_PROTOCOL_BOOT 0x00
#define USB_MOUSE_PROTOCOL_REPORT 0x01
#define USB_CTRL_TIMEOUT 1000000

typedef struct
{
    u8 Buttons;
    s8 X;
    s8 Y;
    s8 Wheel;
} USBMouseReport;

typedef struct
{
    UHCIContext *UHCI;
    u8 DeviceAddress;
    u8 InterruptEndpoint;
    u8 InterruptInterval;
    u16 MaxPacketSize;
    u8 Configured;
    USBMouseReport LastReport;
    u8 Polling;
    UHCIRequest *PollRequest;
    u8 ReportBuffer[8];
} USBMouse;

USBMouse *USBMouseInit(UHCIContext *uhci, u8 port);
void USBMousePoll(USBMouse *mouse);
u8 USBMouseGetButtons(USBMouse *mouse);
s8 USBMouseGetX(USBMouse *mouse);
s8 USBMouseGetY(USBMouse *mouse);
s8 USBMouseGetWheel(USBMouse *mouse);