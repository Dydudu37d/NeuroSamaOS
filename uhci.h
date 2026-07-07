#pragma once

#include "int.h"
#include "pci.h"
#include "str.h"
#include "debug.h"
#include "clock.h"
#include "kmalloc.h"

#define UHCI_CLASS_CODE 0x0C
#define UHCI_SUBCLASS 0x03
#define UHCI_INTERFACE 0x00

#define UHCI_CMD_OFFSET 0x00
#define UHCI_STS_OFFSET 0x02
#define UHCI_INTR_OFFSET 0x04
#define UHCI_FRNUM_OFFSET 0x06
#define UHCI_FRBASE_OFFSET 0x08
#define UHCI_SOFMOD_OFFSET 0x0C
#define UHCI_PORTSC1_OFFSET 0x10
#define UHCI_PORTSC2_OFFSET 0x12

#define UHCI_CMD_RS 0x0001
#define UHCI_CMD_HCRESET 0x0002
#define UHCI_CMD_GRESET 0x0004
#define UHCI_CMD_EGSM 0x0008
#define UHCI_CMD_FGR 0x0010
#define UHCI_CMD_SWDBG 0x0020
#define UHCI_CMD_CF 0x0040
#define UHCI_CMD_MAXP 0x0080

#define UHCI_STS_USBINT 0x0001
#define UHCI_STS_USBERR 0x0002
#define UHCI_STS_RESUME 0x0004
#define UHCI_STS_HSE 0x0008
#define UHCI_STS_HCPE 0x0010
#define UHCI_STS_HCHALTED 0x0020
#define UHCI_STS_SYSERR 0x0010
#define UHCI_STS_PROCERR 0x0020

#define UHCI_INTR_TOCRC 0x0001
#define UHCI_INTR_RESUME 0x0002
#define UHCI_INTR_IOC 0x0004
#define UHCI_INTR_SP 0x0008

#define UHCI_PORTSC_CCS 0x0001
#define UHCI_PORTSC_CSC 0x0002
#define UHCI_PORTSC_PE 0x0004
#define UHCI_PORTSC_PEC 0x0008
#define UHCI_PORTSC_DPLUS 0x0010
#define UHCI_PORTSC_DMINUS 0x0020
#define UHCI_PORTSC_RESERVED 0x0080
#define UHCI_PORTSC_LSDA 0x0100
#define UHCI_PORTSC_RD 0x0040
#define UHCI_PORTSC_PR 0x0200
#define UHCI_PORTSC_SUSP 0x1000
#define UHCI_PORTSC_W1C (UHCI_PORTSC_CSC | UHCI_PORTSC_PEC)

#define UHCI_TD_ACTIVE 0x00800000
#define UHCI_TD_STALL 0x00400000
#define UHCI_TD_CRCERR 0x00040000
#define UHCI_TD_NAK 0x00080000
#define UHCI_TD_BITSTUFF 0x00020000
#define UHCI_TD_STALLED 0x00400000
#define UHCI_TD_DBUFERR 0x00200000
#define UHCI_TD_BABBLE 0x00100000
#define UHCI_TD_CRCTIMEO 0x00040000
#define UHCI_TD_ACTLEN_MASK 0x000007FF
#define UHCI_TD_IOC 0x00010000
#define UHCI_TD_SPD 0x00008000
#define UHCI_TD_MAXLEN_MASK 0x000007FF

#define UHCI_TD_TOKEN_PID_MASK 0x000000FF
#define UHCI_TD_TOKEN_DEVADDR_MASK 0x00007F00
#define UHCI_TD_TOKEN_DEVADDR_SHIFT 8
#define UHCI_TD_TOKEN_ENDPT_MASK 0x00078000
#define UHCI_TD_TOKEN_ENDPT_SHIFT 15
#define UHCI_TD_TOKEN_DATALEN_MASK 0xFFE00000
#define UHCI_TD_TOKEN_DATALEN_SHIFT 21
#define UHCI_TD_TOKEN_TOGGLE_SHIFT 19
#define UHCI_TD_TOKEN_ISO 0x08000000

#define UHCI_FRAME_TERMINATE 0x00000001
#define UHCI_FRAME_QH 0x00000002
#define UHCI_FRAME_ISO 0x00000004
#define UHCI_FRAME_PTR_MASK 0xFFFFFFF0

#define UHCI_QH_VERTICAL 0x00000002
#define UHCI_QH_HORIZONTAL 0x00000001
#define UHCI_QH_PTR_MASK 0xFFFFFFF0

#define UHCI_TD_PID_SETUP 0x2D
#define UHCI_TD_PID_IN 0x69
#define UHCI_TD_PID_OUT 0xE1

#define UHCI_MAX_FRAMES 1024
#define UHCI_FRAME_LIST_SIZE 1024
#define UHCI_MAX_PORTS 2
#define UHCI_MAX_REQUESTS 256
#define UHCI_POLL_TIMEOUT 1000000
#define UHCI_RESET_DELAY_MS 10
#define UHCI_RESUME_DELAY_MS 20
#define UHCI_PORT_RESET_DELAY_MS 50
#define UHCI_PORT_COUNT 2
#define UHCI_FRAME_LIST_BYTES (UHCI_FRAME_LIST_SIZE * 4)
#define UHCI_RECLAIM_32 0
#define UHCI_RECLAIM_64 1

#define UHCI_MAX_ADDRESS 0x100000000ULL

typedef enum
{
    UHCI_OK = 0,
    UHCI_ERROR_GENERAL = -1,
    UHCI_ERROR_TIMEOUT = -2,
    UHCI_ERROR_NO_MEMORY = -3,
    UHCI_ERROR_CRC = -4,
    UHCI_ERROR_BITSTUFF = -5,
    UHCI_ERROR_BABBLE = -6,
    UHCI_ERROR_STALL = -7,
    UHCI_ERROR_BUFFER = -8,
    UHCI_ERROR_PROCESS = -9,
    UHCI_ERROR_SYSTEM = -10,
    UHCI_ERROR_NAK = -11,
    UHCI_ERROR_OVERFLOW = -12
} UHCIResult;

typedef enum
{
    UHCI_TRANSFER_ISOCHRONOUS,
    UHCI_TRANSFER_INTERRUPT,
    UHCI_TRANSFER_CONTROL,
    UHCI_TRANSFER_BULK,
    UHCI_TRANSFER_SETUP,
    UHCI_TRANSFER_IN,
    UHCI_TRANSFER_OUT
} UHCITransferType;

typedef struct UHCIHostController
{
    u16 IOBase;
    u8 Bus;
    u8 Slot;
    u8 Func;
    u8 PortCount;
    u32 FrameListPhys;
    void *FrameListVirt;
    u16 FrameNumber;
    u8 Running;
    u8 Configured;
    u8 Suspended;
    u8 GlobalSuspend;
    u8 ResumeSignaling;
    u8 MaxPacket;
    u8 DebugMode;
    AllocPool *MemoryPool;
} UHCIHostController;

typedef struct UHCIFrameListEntry
{
    u32 Pointer;
} UHCIFrameListEntry;

typedef struct __attribute__((aligned(16))) UHCITransferDescriptor
{
    u32 LinkPointer;
    u32 ControlStatus;
    u32 Token;
    u32 BufferPointer;
    u32 Reserved[4];
}__attribute__((aligned(16))) UHCITransferDescriptor;

typedef struct __attribute__((aligned(16))) UHCIQueueHead
{
    u32 HorizontalLink;
    u32 VerticalLink;
} UHCIQueueHead;

typedef struct UHCIPortStatus
{
    u8 CurrentConnect;
    u8 ConnectChange;
    u8 Enabled;
    u8 EnableChange;
    u8 LowSpeed;
    u8 Suspended;
    u8 ResumeDetect;
    u8 PortReset;
    u8 LineStatusDPlus;
    u8 LineStatusDMinus;
} UHCIPortStatus;

typedef struct UHCIContext UHCIContext;
typedef struct UHCIRequest
{
    u8 DeviceAddress;
    u8 Endpoint;
    u8 Direction;
    UHCITransferType Type;
    u8 *DataBuffer;
    u16 DataLength;
    u16 MaxPacketSize;
    u8 ErrorCount;
    u8 Active;
    u8 Completed;
    u8 Success;
    u8 Stalled;
    u16 ActualLength;
    u8 FrameNumber;
    UHCIResult Error;
    UHCIResult Result;
    void (*Callback)(struct UHCIRequest *req);
    void *UserData;
    UHCITransferDescriptor *TD;
    UHCITransferDescriptor *TDList;
    UHCIQueueHead *QH;
    struct UHCIRequest *Next;
    u8 TDCount;
    u32 DataPhys;
    u32 TDPhys;
    u32 QHPhys;
    u16 FrameIndex;
    UHCIContext *UHCI;
} UHCIRequest;

typedef struct UHCIContext
{
    UHCIHostController *HC;
    UHCIRequest *PendingRequests;
    UHCIRequest *CompletedRequests;
    UHCIRequest *CompletedRequestsTail;
    UHCIRequest *FreeRequests;
    AllocPool *MemoryPool;
    UHCIFrameListEntry *FrameList;
    u64 FrameListSize;
    u64 MaxRequests;
    u8 Initialized;
    u8 PollMode;
    u32 FrameListPhys;
    void *FrameListVirt;
    u8 Suspended;
    u8 GlobalSuspend;
    u8 ResumeSignaling;
    void (*HandleCompletion)(UHCIRequest *req);
    void (*HandleError)(UHCIRequest *req, UHCIResult error);
    void (*HandlePortChange)(u8 port, u16 status);
    u32 TSCPerMs;
    u8 IsPolling;
} UHCIContext;

typedef struct {
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
} __attribute__((packed)) USBDeviceDescriptor;

typedef struct {
    u8 bLength;
    u8 bDescriptorType;
    u16 wTotalLength;
    u8 bNumInterfaces;
    u8 bConfigurationValue;
    u8 iConfiguration;
    u8 bmAttributes;
    u8 bMaxPower;
} __attribute__((packed)) USBConfigDescriptor;

typedef struct {
    u8 bLength;
    u8 bDescriptorType;
    u8 bEndpointAddress;
    u8 bmAttributes;
    u16 wMaxPacketSize;
    u8 bInterval;
} __attribute__((packed)) USBEndpointDescriptor;

typedef struct
{
    u8 bLength;
    u8 bDescriptorType;
    u8 bInterfaceNumber;
    u8 bAlternateSetting;
    u8 bNumEndpoints;
    u8 bInterfaceClass;
    u8 bInterfaceSubClass;
    u8 bInterfaceProtocol;
    u8 iInterface;
} __attribute__((packed)) USBInterfaceDescriptor;

UHCIHostController *UHCICreate(u8 Bus, u8 Slot, u8 Func, AllocPool *Pool);
UHCIResult UHCIInitialize(UHCIContext *ctx);
UHCIResult UHCIStart(UHCIContext *ctx);
UHCIResult UHCIStop(UHCIContext *ctx);
UHCIResult UHCIReset(UHCIContext *ctx);
UHCIResult UHCIConfigure(UHCIContext *ctx, u8 maxPacket, u8 debugMode);
UHCIRequest *UHCICreateRequest(UHCIContext *ctx);
UHCIResult UHCISubmitRequest(UHCIContext *ctx, UHCIRequest *req);
UHCIResult UHCICancelRequest(UHCIContext *ctx, UHCIRequest *req);
UHCIResult UHCIPoll(UHCIContext *ctx);
u8 UHCIIsHalted(UHCIContext *ctx);
u8 UHCIIsRunning(UHCIContext *ctx);
u16 UHCIGetFrameNumber(UHCIContext *ctx);
UHCIResult UHCISetFrameNumber(UHCIContext *ctx, u16 Frame);
u16 UHCIGetPortStatus(UHCIContext *ctx, u8 Port);
UHCIResult UHCIResetPort(UHCIContext *ctx, u8 Port);
UHCIResult UHCIEnablePort(UHCIContext *ctx, u8 Port);
UHCIResult UHCIDisablePort(UHCIContext *ctx, u8 Port);
UHCIResult UHCISuspendPort(UHCIContext *ctx, u8 Port);
UHCIResult UHCIResumePort(UHCIContext *ctx, u8 Port);
UHCIResult UHCIClearPortChange(UHCIContext *ctx, u8 Port);
void UHCIDumpStatus(UHCIContext *ctx);
void UHCIDumpRequests(UHCIContext *ctx);
void UHCIDumpFrameList(UHCIContext *ctx, u16 start, u16 count);
void UHCIClearFrameListEntry(UHCIContext* ctx, UHCIQueueHead* qh);
UHCIResult ExecuteControlTransfer(UHCIContext *ctx, u8 devAddr, u8 endpoint, u8 *setupPacket, u16 setupLen, u8 *data, u16 dataLen, u8 dir, u8 is_low_speed, u16 maxPacketSize);