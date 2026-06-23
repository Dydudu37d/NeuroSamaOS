#pragma once

#include "int.h"

//=============================================================================
//  Pixel Format Definitions
//=============================================================================
#define PixelRedGreenBlueReserved8BitPerColor   0
#define PixelBlueGreenRedReserved8BitPerColor   1
#define PixelBitMask                             2
#define PixelBltOnly                             3
#define PixelFormatMax                           4

//=============================================================================
//  Calling Convention
//=============================================================================
#define EFIAPI __attribute__((ms_abi))

//=============================================================================
//  GUID Definitions
//=============================================================================
#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID { 0x9042A9DE, 0x23DC, 0x4A38, { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } }

//=============================================================================
//  Basic Types
//=============================================================================
typedef u64   EFI_STATUS;
typedef u64   EFI_HANDLE;
typedef u64   UINTN;
typedef s64   INTN;
typedef u64   EFI_PHYSICAL_ADDRESS;

//=============================================================================
//  EFI_STATUS Error Codes
//=============================================================================
#define EFI_SUCCESS                   0
#define EFI_LOAD_ERROR                ((EFI_STATUS)(1ULL << 63) | 1)
#define EFI_INVALID_PARAMETER         ((EFI_STATUS)(1ULL << 63) | 2)
#define EFI_UNSUPPORTED               ((EFI_STATUS)(1ULL << 63) | 3)
#define EFI_BAD_BUFFER_SIZE           ((EFI_STATUS)(1ULL << 63) | 4)
#define EFI_BUFFER_TOO_SMALL          ((EFI_STATUS)(1ULL << 63) | 5)
#define EFI_NOT_READY                 ((EFI_STATUS)(1ULL << 63) | 6)
#define EFI_DEVICE_ERROR              ((EFI_STATUS)(1ULL << 63) | 7)
#define EFI_WRITE_PROTECTED           ((EFI_STATUS)(1ULL << 63) | 8)
#define EFI_OUT_OF_RESOURCES          ((EFI_STATUS)(1ULL << 63) | 9)
#define EFI_VOLUME_CORRUPTED          ((EFI_STATUS)(1ULL << 63) | 10)
#define EFI_VOLUME_FULL               ((EFI_STATUS)(1ULL << 63) | 11)
#define EFI_NO_MEDIA                  ((EFI_STATUS)(1ULL << 63) | 12)
#define EFI_MEDIA_CHANGED             ((EFI_STATUS)(1ULL << 63) | 13)
#define EFI_NOT_FOUND                 ((EFI_STATUS)(1ULL << 63) | 14)
#define EFI_ACCESS_DENIED             ((EFI_STATUS)(1ULL << 63) | 15)
#define EFI_TIMEOUT                   ((EFI_STATUS)(1ULL << 63) | 16)
#define EFI_ABORTED                   ((EFI_STATUS)(1ULL << 63) | 17)
#define EFI_SECURITY_VIOLATION        ((EFI_STATUS)(1ULL << 63) | 26)
#define EFI_ERROR(status) (((INTN)(status)) < 0)
//=============================================================================
//  Memory Types (for AllocatePool/AllocatePages)
//=============================================================================
#define EfiReservedMemoryType       0
#define EfiLoaderCode               1
#define EfiLoaderData               2
#define EfiBootServicesCode         3
#define EfiBootServicesData         4
#define EfiRuntimeServicesCode      5
#define EfiRuntimeServicesData      6
#define EfiConventionalMemory       7
#define EfiMaxMemoryType            8

#define EFI_PAGE_SIZE 4096
#define EFI_SIZE_TO_PAGES(x) (((x) + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE)


//=============================================================================
//  Allocate Types (for AllocatePages)
//=============================================================================
typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

//=============================================================================
//  EFI Table Header
//=============================================================================
typedef struct {
    u64 Signature;
    u32 Revision;
    u32 HeaderSize;
    u32 CRC32;
    u32 Reserved;
} EFI_TABLE_HEADER;

//=============================================================================
//  EFI GUID
//=============================================================================
typedef struct {
    u32 Data1;
    u16 Data2;
    u16 Data3;
    u8  Data4[8];
} EFI_GUID;

static const EFI_GUID gGopGuid = {
    0x9042A9DE,
    0x23DC,
    0x4A38,
    { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A }
};

//=============================================================================
//  EFI Boot Services (Partial - GOP Required Only)
//=============================================================================
typedef struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;
    
    u64 (EFIAPI *RaiseTPL)(u64 NewTpl);
    void (EFIAPI *RestoreTPL)(u64 OldTpl);
    
    EFI_STATUS (EFIAPI *AllocatePages)(u64 Type, u64 MemoryType, u64 Pages, u64 *Memory);
    EFI_STATUS (EFIAPI *FreePages)(u64 Memory, u64 Pages);
    EFI_STATUS (EFIAPI *GetMemoryMap)(u64 *MemoryMapSize, void *MemoryMap, u64 *MapKey, u64 *DescriptorSize, u32 *DescriptorVersion);
    EFI_STATUS (EFIAPI *AllocatePool)(u64 PoolType, u64 Size, void **Buffer);
    EFI_STATUS (EFIAPI *FreePool)(void *Buffer);
    
    EFI_STATUS (EFIAPI *CreateEvent)(u64 Type, u64 NotifyTpl, void (EFIAPI *NotifyFunction)(u64 Event, void *Context), void *NotifyContext, u64 *Event);
    EFI_STATUS (EFIAPI *SetTimer)(u64 Event, u64 Type, u64 TriggerTime);
    EFI_STATUS (EFIAPI *WaitForEvent)(u64 NumberOfEvents, u64 *Events, u64 *Index);
    EFI_STATUS (EFIAPI *SignalEvent)(u64 Event);
    EFI_STATUS (EFIAPI *CloseEvent)(u64 Event);
    EFI_STATUS (EFIAPI *CheckEvent)(u64 Event);
    
    EFI_STATUS (EFIAPI *InstallProtocolInterface)(u64 *Handle, EFI_GUID *Protocol, u64 InterfaceType, void *Interface);
    EFI_STATUS (EFIAPI *ReinstallProtocolInterface)(u64 Handle, EFI_GUID *Protocol, void *OldInterface, void *NewInterface);
    EFI_STATUS (EFIAPI *UninstallProtocolInterface)(u64 Handle, EFI_GUID *Protocol, void *Interface);
    EFI_STATUS (EFIAPI *HandleProtocol)(u64 Handle, EFI_GUID *Protocol, void **Interface);
    void *Reserved;
    EFI_STATUS (EFIAPI *RegisterProtocolNotify)(EFI_GUID *Protocol, u64 Event, u64 *Registration);
    EFI_STATUS (EFIAPI *LocateHandle)(u64 SearchType, EFI_GUID *Protocol, void *SearchKey, u64 *BufferSize, u64 *Buffer);
    EFI_STATUS (EFIAPI *LocateDevicePath)(EFI_GUID *Protocol, void **DevicePath, u64 *DevicePathInstance);
    EFI_STATUS (EFIAPI *InstallConfigurationTable)(EFI_GUID *Guid, void *Table);
    
    EFI_STATUS (EFIAPI *LoadImage)(u8 BootPolicy, u64 ParentImageHandle, void *DevicePath, void *SourceBuffer, u64 SourceSize, u64 *ImageHandle);
    EFI_STATUS (EFIAPI *StartImage)(u64 ImageHandle, u64 *ExitDataSize, u16 **ExitData);
    EFI_STATUS (EFIAPI *Exit)(u64 ImageHandle, EFI_STATUS ExitStatus, u64 ExitDataSize, u16 *ExitData);
    EFI_STATUS (EFIAPI *UnloadImage)(u64 ImageHandle);
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE ImageHandle, u64 MapKey);
    
    EFI_STATUS (EFIAPI *GetNextMonotonicCount)(u64 *Count);
    void (EFIAPI *Stall)(u64 Microseconds);
    EFI_STATUS (EFIAPI *SetWatchdogTimer)(u64 Timeout, u64 WatchdogCode, u64 DataSize, u16 *WatchdogData);
    
    EFI_STATUS (EFIAPI *ConnectController)(u64 ControllerHandle, u64 *DriverImageHandle, void *RemainingDevicePath, u8 Recursive);
    EFI_STATUS (EFIAPI *DisconnectController)(u64 ControllerHandle, u64 DriverImageHandle, u64 ChildHandle);
    
    EFI_STATUS (EFIAPI *OpenProtocol)(u64 Handle, EFI_GUID *Protocol, void **Interface, u64 AgentHandle, u64 ControllerHandle, u32 Attributes);
    EFI_STATUS (EFIAPI *CloseProtocol)(u64 Handle, EFI_GUID *Protocol, u64 AgentHandle, u64 ControllerHandle);
    EFI_STATUS (EFIAPI *OpenProtocolInformation)(u64 Handle, EFI_GUID *Protocol, void **EntryBuffer, u64 *EntryCount);
    EFI_STATUS (EFIAPI *ProtocolsPerHandle)(u64 Handle, void **ProtocolBuffer, u64 *ProtocolBufferCount);
    EFI_STATUS (EFIAPI *LocateHandleBuffer)(u64 SearchType, EFI_GUID *Protocol, void *SearchKey, u64 *NumberOfHandles, u64 **Buffer);
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface);
} EFI_BOOT_SERVICES;

//=============================================================================
//  Simple Text Output Protocol (for debug output)
//=============================================================================
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, u8 ExtendedVerification);
    EFI_STATUS (EFIAPI *OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, u16 *String);
    EFI_STATUS (EFIAPI *TestString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, u16 *String);
    EFI_STATUS (EFIAPI *QueryMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, u32 ModeNumber, u32 *Columns, u32 *Rows);
    EFI_STATUS (EFIAPI *SetMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, u32 ModeNumber);
    EFI_STATUS (EFIAPI *SetAttribute)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, u32 Attribute);
    EFI_STATUS (EFIAPI *ClearScreen)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
    EFI_STATUS (EFIAPI *SetCursorPosition)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, u32 Column, u32 Row);
    EFI_STATUS (EFIAPI *EnableCursor)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, u8 Enable);
    
    // Current Mode Data
    u32 Mode;
    u32 Attribute;
    u32 CursorColumn;
    u32 CursorRow;
    u8 CursorVisible;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

//=============================================================================
//  EFI System Table
//=============================================================================
typedef struct {
    EFI_TABLE_HEADER Hdr;
    u16 *FirmwareVendor;
    u32 FirmwareRevision;
    u32 _padding;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;  // Fixed: was void*
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

//=============================================================================
//  Graphics Output Protocol (GOP) Structures
//=============================================================================

// GOP Mode Information
typedef struct {
    u32 RedMask;
    u32 GreenMask;
    u32 BlueMask;
    u32 ReservedMask;
} EFI_PIXEL_BITMAP;

typedef struct {
    u32 Version;
    u32 HorizontalResolution;
    u32 VerticalResolution;
    u32 PixelFormat;
    EFI_PIXEL_BITMAP PixelInformation;  // Fixed: was void*, now proper struct
    u32 PixelsPerScanLine;
} EFI_GOP_MODE_INFO;

// GOP Mode (current state)
typedef struct {
    u32 MaxMode;
    u32 Mode;
    EFI_GOP_MODE_INFO *Info;  // Fixed: was void*, now proper pointer type
    UINTN SizeOfInfo;         // Fixed: was u64, now UINTN
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GOP_MODE;

// GOP Protocol
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *QueryMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *This, 
                                   u32 ModeNumber, 
                                   UINTN *SizeOfInfo,           // Fixed: semantics clarified
                                   EFI_GOP_MODE_INFO **Info);
    
    EFI_STATUS (EFIAPI *SetMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *This, 
                                 u32 ModeNumber);
    
    EFI_STATUS (EFIAPI *Blt)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
                             void *BltBuffer,
                             u32 BltOperation,
                             u32 SourceX,
                             u32 SourceY,
                             u32 DestinationX,
                             u32 DestinationY,
                             u32 Width,
                             u32 Height,
                             u32 Delta);
    
    EFI_GOP_MODE *Mode;  // Fixed: was void*, now proper pointer type
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

// MemoryDescriptor
typedef struct {
    u32 Type;
    u64 PhysicalStart;
    u64 VirtualStart;
    u64 NumberOfPages;
    u64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

//=============================================================================
//  Loaded Image Protocol (Full Definition)
//=============================================================================
static const EFI_GUID gEfiLoadedImageProtocolGuid = {
    0x5B1B31A1,
    0x9562,
    0x11D2,
    { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

typedef struct {
    u32        Revision;
    u32        _pad0;
    EFI_HANDLE ParentHandle;
    void       *SystemTable;
    EFI_HANDLE DeviceHandle;
    void       *FilePath;
    void       *Reserved;
    u32        LoadOptionsSize;
    u32        _pad1;
    void       *LoadOptions;
    void       *ImageBase;
    u64        ImageSize;
    u64        ImageMemoryType;
    EFI_STATUS (EFIAPI *Unload)(EFI_HANDLE ImageHandle);
} EFI_LOADED_IMAGE_PROTOCOL;
