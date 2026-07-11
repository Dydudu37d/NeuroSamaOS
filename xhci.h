#pragma once

#include "int.h"

struct MouseDevice;
struct KeyboardDevice;

typedef struct XhciCapRegs {
    u8 CapLength;
    u8 Reserved;
    u16 HciVersion;
    u32 Hcsparams1;
    u32 Hcsparams2;
    u32 Hcsparams3;
    u32 Hccparams1;
    u32 Dboff;
    u32 Rtsoff;
} __attribute__((packed)) XhciCapRegs;

typedef struct XhciOpRegs {
    u32 UsbCmd;
    u32 UsbSts;
    u32 Pagesize;
    u32 Reserved1[2];
    u32 Dnctrl;
    u64 Crcr;
    u32 Reserved2[4];
    u64 Dcbaap;
    u32 Config;
} __attribute__((packed)) XhciOpRegs;

typedef struct XhciPortRegs {
    u32 Portsc;
    u32 Portpmsc;
    u32 Portli;
    u32 Prtcc;
} __attribute__((packed)) XhciPortRegs;

typedef struct XhciDcbaa {
    u64 DevCtxPtr[256];
} __attribute__((packed)) XhciDcbaa;

typedef struct XhciSlotContext {
    u32 DevInfo;
    u32 DevInfo2;
    u32 TtInfo;
    u32 DevState;
    u32 Reserved[4];
} __attribute__((packed)) XhciSlotContext;

typedef struct XhciEndpointContext {
    u32 EpInfo;
    u32 EpInfo2;
    u32 DequeueLow;
    u32 DequeueHigh;
    u32 Reserved[4];
} __attribute__((packed)) XhciEndpointContext;

typedef struct XhciInputControlContext {
    u32 DropFlags;
    u32 AddFlags;
    u32 Reserved[6];
} __attribute__((packed)) XhciInputControlContext;

typedef struct XhciTrb {
    u32 ParameterLow;
    u32 ParameterHigh;
    u32 Status;
    u32 Control;
} __attribute__((packed)) XhciTrb;

typedef struct XhciInputContext {
    XhciInputControlContext InputCtrl;
    XhciSlotContext Slot;
    XhciEndpointContext Ep[31];
} __attribute__((packed)) XhciInputContext;

typedef struct XhciDeviceContext {
    XhciSlotContext Slot;
    XhciEndpointContext Ep[31];
} __attribute__((packed)) XhciDeviceContext;

typedef struct {
    XhciTrb* Ring;
    u32 Enqueue;
    u32 Cycle;
    u32 RingSize;
    u64 RingPhys;
} XhciEpRing;

typedef struct XhciController {
    u64 MmioBase;
    XhciCapRegs* Cap;
    XhciOpRegs* Op;
    XhciPortRegs* Ports;
    u64 Dboff;
    u64 Rtsoff;
    u64 DoorbellBase;
    u64 RtBase;
    u8 MaxSlots;
    u8 Ccs;
    u32 ErstSize;
    XhciDcbaa* Dcbaa;
    XhciTrb* CmdRing;
    XhciTrb* EvRing;
    u64* Erst;
    u32 CmdEnqueue;
    u32 CmdCycle;
    u32 EvDequeue;
    u32 EvCycle;
    u32 EvRingSize;
    u32 CmdRingSize;
    
    XhciEpRing EpRings[256];
} XhciController;

typedef struct UsbDeviceDescriptor {
    u8 bLength;
    u8 bDescriptorType;
    u16 bcdUSB;
    u8 bDeviceClass;
    u8 bDeviceSubClass;
    u8 bDeviceProtocol;
    u8 bMaxPacketSize0;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8 iManufacturer;
    u8 iProduct;
    u8 iSerialNumber;
    u8 bNumConfigurations;
} __attribute__((packed)) UsbDeviceDescriptor;

typedef struct UsbConfigDescriptor {
    u8 bLength;
    u8 bDescriptorType;
    u16 wTotalLength;
    u8 bNumInterfaces;
    u8 bConfigurationValue;
    u8 iConfiguration;
    u8 bmAttributes;
    u8 bMaxPower;
} __attribute__((packed)) UsbConfigDescriptor;

typedef struct UsbInterfaceDescriptor {
    u8 bLength;
    u8 bDescriptorType;
    u8 bInterfaceNumber;
    u8 bAlternateSetting;
    u8 bNumEndpoints;
    u8 bInterfaceClass;
    u8 bInterfaceSubClass;
    u8 bInterfaceProtocol;
    u8 iInterface;
} __attribute__((packed)) UsbInterfaceDescriptor;

typedef struct UsbEndpointDescriptor {
    u8 bLength;
    u8 bDescriptorType;
    u8 bEndpointAddress;
    u8 bmAttributes;
    u16 wMaxPacketSize;
    u8 bInterval;
} __attribute__((packed)) UsbEndpointDescriptor;

#define USB_CLASS_HID 0x03
#define USB_SUBCLASS_BOOT 0x01
#define USB_PROTOCOL_KEYBOARD 0x01
#define USB_PROTOCOL_MOUSE 0x02

#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_DT_DEVICE 0x01
#define USB_DT_CONFIG 0x02
#define USB_DT_INTERFACE 0x04
#define USB_DT_ENDPOINT 0x05

#define XHCI_TRB_SETUP_STAGE   2
#define XHCI_TRB_DATA_STAGE    3
#define XHCI_TRB_STATUS_STAGE  4

#define XHCI_PORTSC_CCS (1 << 0)
#define XHCI_PORTSC_PED (1 << 1)
#define XHCI_PORTSC_PR (1 << 4)
#define XHCI_PORTSC_PLS_SHIFT 5
#define XHCI_PORTSC_PLS_MASK 0xF
#define XHCI_PORTSC_PP (1 << 9)
#define XHCI_PORTSC_CSC (1 << 17)
#define XHCI_PORTSC_WRC (1 << 19)
#define XHCI_PORTSC_PRC (1 << 21)
#define XHCI_PORTSC_PEC (1 << 22)

#define XHCI_PORTSPEED_FULL 1
#define XHCI_PORTSPEED_LOW 2
#define XHCI_PORTSPEED_HIGH 3
#define XHCI_PORTSPEED_SUPER 4

#define XHCI_TRB_C 0x01
#define XHCI_TRB_TC 0x02
#define XHCI_TRB_CHAIN 0x10
#define XHCI_TRB_IOC 0x20
#define XHCI_TRB_IDT 0x40

#define XHCI_TRB_NORMAL 1
#define XHCI_TRB_SETUP 2
#define XHCI_TRB_DATA 3
#define XHCI_TRB_STATUS 4
#define XHCI_TRB_LINK 6
#define XHCI_TRB_ENABLE_SLOT 9
#define XHCI_TRB_DISABLE_SLOT 10
#define XHCI_TRB_ADDRESS_DEVICE 11
#define XHCI_TRB_CONFIGURE_ENDPOINT 12

#define XHCI_TRB_TRANSFER_EVENT 32
#define XHCI_TRB_COMMAND_COMPLETE 33
#define XHCI_TRB_PORT_STATUS_CHANGE 34

#define XHCI_CMPLT_SUCCESS 1
#define XHCI_CMPLT_SHORT_PACKET 13
#define XHCI_CMPLT_STALL 20
#define XHCI_CMPLT_BABBLE 22

extern struct MouseDevice* ActiveMouse;
extern struct KeyboardDevice* ActiveKbd;
extern u64 XHCI_MAX_PORTS;

u64 XhciGetMmioBase(u8 Bus, u8 Slot, u8 Func);
XhciController* XhciInit(u64 MmioBase);
u8 XhciReadEvent(XhciController* Xhci, XhciTrb* Event);
u8 XhciEnableSlot(XhciController* Xhci);
u8 XhciAddressDevice(XhciController* Xhci, u8 SlotId, XhciInputContext* InputCtx, u8 bsr);
void XhciRingDoorbell(XhciController* Xhci, u8 SlotId, u8 EpId);
void XhciSendCommand(XhciController* Xhci, u64 Param, u32 Status, u8 Type, u8 SlotId);
void XhciScanPorts(XhciController* Xhci);
u8 XhciEnumerateDevice(XhciController* Xhci, u8 PortId, u8 Speed);
u8 XhciControlTransfer(XhciController* Xhci, u8 SlotId, u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, void* Buffer);
void XhciGetDeviceDescriptor(XhciController* Xhci, u8 SlotId, UsbDeviceDescriptor* Desc);
void XhciParseDevice(XhciController* Xhci, u8 SlotId);
void XhciPollEvents(XhciController* Xhci);
