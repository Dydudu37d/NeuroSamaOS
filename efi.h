#pragma once

#include "int.h"

#define PixelRedGreenBlueReserved8BitPerColor   0
#define PixelBlueGreenRedReserved8BitPerColor   1
#define PixelBitMask                             2
#define PixelBltOnly                             3
#define PixelFormatMax                           4

#define EFIAPI __attribute__((ms_abi))

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID { 0x9042A9DE, 0x23DC, 0x4A38, { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } }
#define EFI_HII_FONT_PROTOCOL_GUID { 0xe9ca4775, 0x8657, 0x47fc, { 0x97, 0xe7, 0x7e, 0xd6, 0x5a, 0x8, 0x43, 0x24 } }

typedef u64   EFI_STATUS;
typedef u64   EFI_HANDLE;
typedef u64   UINTN;
typedef s64   INTN;
typedef u64   EFI_PHYSICAL_ADDRESS;

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

#define EFI_UNSPECIFIED_TIMEZONE 0x07FF

#define EFI_VARIABLE_NON_VOLATILE                           0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS                     0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS                         0x00000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD                  0x00000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS             0x00000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS  0x00000020
#define EFI_VARIABLE_APPEND_WRITE                           0x00000040
#define EFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS          0x00000080

#define CAPSULE_FLAGS_PERSIST_ACROSS_RESET  0x00010000
#define CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE 0x00020000
#define CAPSULE_FLAGS_INITIATE_RESET        0x00040000

#define SCAN_NULL       0x00
#define SCAN_UP         0x01
#define SCAN_DOWN       0x02
#define SCAN_RIGHT      0x03
#define SCAN_LEFT       0x04
#define SCAN_HOME       0x05
#define SCAN_END        0x06
#define SCAN_INSERT     0x07
#define SCAN_DELETE     0x08
#define SCAN_PAGE_UP    0x09
#define SCAN_PAGE_DOWN  0x0A
#define SCAN_F1         0x0B
#define SCAN_F2         0x0C
#define SCAN_F3         0x0D
#define SCAN_F4         0x0E
#define SCAN_F5         0x0F
#define SCAN_F6         0x10
#define SCAN_F7         0x11
#define SCAN_F8         0x12
#define SCAN_F9         0x13
#define SCAN_F10        0x14
#define SCAN_ESC        0x17

#define EfiReservedMemoryType       0
#define EfiLoaderCode               1
#define EfiLoaderData               2
#define EfiBootServicesCode         3
#define EfiBootServicesData         4
#define EfiRuntimeServicesCode      5
#define EfiRuntimeServicesData      6
#define EfiConventionalMemory       7
#define EfiMaxMemoryType            8
#define EfiACPIReclaimMemory        9
#define EfiACPIMemoryNVS            10
#define EfiMemoryMappedIO           11
#define EfiMemoryMappedIOPortSpace  12
#define EfiPalCode                  13

#define EFI_PAGE_SIZE 4096
#define EFI_SIZE_TO_PAGES(x) (((x) + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE)

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

typedef struct {
    u64 Signature;
    u32 Revision;
    u32 HeaderSize;
    u32 CRC32;
    u32 Reserved;
} EFI_TABLE_HEADER;

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
    
    u32 Mode;
    u32 Attribute;
    u32 CursorColumn;
    u32 CursorRow;
    u8 CursorVisible;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    u16 ScanCode;
    u16 UnicodeChar;
} EFI_INPUT_KEY;

typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(
        EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
        u8                             ExtendedVerification
    );
    EFI_STATUS (EFIAPI *ReadKeyStroke)(
        EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
        EFI_INPUT_KEY                  *Key
    );
    u64 WaitForKey; 
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

typedef struct {
    EFI_GUID VendorGuid;
    void     *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
    u16 Year;
    u8  Month;
    u8  Day;
    u8  Hour;
    u8  Minute;
    u8  Second;
    u8  Pad1;
    u32 Nanosecond;
    s16 TimeZone;
    u8  Daylight;
    u8  Pad2;
} EFI_TIME;

typedef struct {
    u32 Resolution;
    u32 Accuracy;
    u8  SetsToZero;
} EFI_TIME_CAPABILITIES;

typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
    EfiResetPlatformSpecific
} EFI_RESET_TYPE;

typedef struct {
    EFI_GUID CapsuleGuid;
    u32      HeaderSize;
    u32      Flags;
    u32      CapsuleImageSize;
} EFI_CAPSULE_HEADER;

typedef struct EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER                Hdr;
    EFI_STATUS (EFIAPI *GetTime)(EFI_TIME *Time, EFI_TIME_CAPABILITIES *Capabilities);
    EFI_STATUS (EFIAPI *SetTime)(EFI_TIME *Time);
    EFI_STATUS (EFIAPI *GetWakeupTime)(u8 *Enabled, u8 *Pending, EFI_TIME *Time);
    EFI_STATUS (EFIAPI *SetWakeupTime)(u8 Enable, EFI_TIME *Time);
    EFI_STATUS (EFIAPI *SetVirtualAddressMap)(UINTN MemoryMapSize, UINTN DescriptorSize, u32 DescriptorVersion, void *VirtualMap);
    EFI_STATUS (EFIAPI *ConvertPointer)(UINTN DebugDisposition, void **Address);
    EFI_STATUS (EFIAPI *GetVariable)(u16 *VariableName, EFI_GUID *VendorGuid, u32 *Attributes, UINTN *DataSize, void *Data);
    EFI_STATUS (EFIAPI *GetNextVariableName)(UINTN *VariableNameSize, u16 *VariableName, EFI_GUID *VendorGuid);
    EFI_STATUS (EFIAPI *SetVariable)(u16 *VariableName, EFI_GUID *VendorGuid, u32 Attributes, UINTN DataSize, void *Data);
    EFI_STATUS (EFIAPI *GetNextHighMonotonicCount)(u32 *HighCount);
    EFI_STATUS (EFIAPI *ResetSystem)(EFI_RESET_TYPE ResetType, EFI_STATUS ResetStatus, UINTN DataSize, void *ResetData);
    EFI_STATUS (EFIAPI *UpdateCapsule)(EFI_CAPSULE_HEADER **CapsuleHeaderArray, UINTN CapsuleCount, EFI_PHYSICAL_ADDRESS ScatterGatherList);
    EFI_STATUS (EFIAPI *QueryCapsuleCapabilities)(EFI_CAPSULE_HEADER **CapsuleHeaderArray, UINTN CapsuleCount, u64 *MaximumCapsuleSize, EFI_RESET_TYPE *ResetType);
    EFI_STATUS (EFIAPI *QueryVariableInfo)(u32 Attributes, u64 *MaximumVariableStorageSize, u64 *RemainingVariableStorageSize, u64 *MaximumVariableSize);
} EFI_RUNTIME_SERVICES;

typedef struct EFI_SYSTEM_TABLE{
    EFI_TABLE_HEADER Hdr;
    u16 *FirmwareVendor;
    u32 FirmwareRevision;
    u32 _padding;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;

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
    EFI_PIXEL_BITMAP PixelInformation;
    u32 PixelsPerScanLine;
} EFI_GOP_MODE_INFO;

typedef struct {
    u32 MaxMode;
    u32 Mode;
    EFI_GOP_MODE_INFO *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GOP_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *QueryMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *This, 
                                   u32 ModeNumber, 
                                   UINTN *SizeOfInfo,
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
    
    EFI_GOP_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    u32 Type;
    u64 PhysicalStart;
    u64 VirtualStart;
    u64 NumberOfPages;
    u64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

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

static inline _Bool CompareGuid(EFI_GUID* a, EFI_GUID* b) {
    if (a->Data1 != b->Data1) return 0;
    if (a->Data2 != b->Data2) return 0;
    if (a->Data3 != b->Data3) return 0;
    for (int i = 0; i < 8; i++) {
        if (a->Data4[i] != b->Data4[i]) return 0;
    }
    return 1;
}