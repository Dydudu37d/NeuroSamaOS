#pragma once

#include "uhci.h"

#define USB_MOUSE_REPORT_SIZE 4

typedef struct {
    u8 Buttons;
    u8 X;
    u8 Y;
    u8 Wheel;
} USBMouseReport;

typedef struct {
    UHCIContext *UHCI;
    u8 DeviceAddress;
    u8 InterruptEndpoint;
    u8 InterruptInterval;
    u16 MaxPacketSize;
    u8 Configured;
    USBMouseReport LastReport;
    u8 Polling;
    UHCIRequest *PollRequest;
    u8 ReportBuffer[USB_MOUSE_REPORT_SIZE];
    u8 IsPolling;
    u8 IsLowSpeed;
    u8 MaxPacketSize0;
} USBMouse;

USBMouse* USBMouseInit(UHCIContext *uhci, u8 port);
void USBMousePoll(USBMouse *mouse);
u8 USBMouseGetButtons(USBMouse *mouse);
s8 USBMouseGetX(USBMouse *mouse);
s8 USBMouseGetY(USBMouse *mouse);
s8 USBMouseGetWheel(USBMouse *mouse);
void USBMouseCleanup(USBMouse *mouse);