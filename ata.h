#pragma once
#include "int.h"

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECTOR_CNT  0x02
#define ATA_REG_LBA_LOW     0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HIGH    0x05
#define ATA_REG_DEVICE      0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30

#define ATA_STATUS_BSY      0x80
#define ATA_STATUS_DRDY     0x40
#define ATA_STATUS_DF       0x20
#define ATA_STATUS_DRQ      0x08
#define ATA_STATUS_ERR      0x01

#define ATA_CMD_READ_PIO_EXT  0x24
#define ATA_CMD_WRITE_PIO_EXT 0x34
#define ATA_CMD_IDENTIFY       0xEC

typedef struct {
    _Bool Present;
    _Bool SupportsLBA48;
    u64 TotalSectors;
    u32 MaxLBA28;
} ATADriveInfo;

extern ATADriveInfo ATADrives[2][2];

typedef struct {
    u16 Data[256];
    u16 Error;
    u16 SectorCount;
    u16 LBALow;
    u16 LBAMid;
    u16 LBAHigh;
    u16 Device;
    u16 Status;
    u16 Command;
} ATAChannel;

_Bool ATAInit();
_Bool ATADetectDrive(u8 Channel, u8 Drive);
_Bool ATAReadSector(u8 Channel, u8 Drive, u64 LBA, u8* Buffer);
_Bool ATAWriteSector(u8 Channel, u8 Drive, u64 LBA, u8* Buffer);
_Bool ATADetectDrive48(u8 Channel, u8 Drive);
_Bool ATAReadSector48(u8 Channel, u8 Drive, u64 LBA, u8* Buffer);
_Bool ATAWriteSector48(u8 Channel, u8 Drive, u64 LBA, u8* Buffer);
_Bool ATADetectDrive28(u8 Channel, u8 Drive);
_Bool ATAReadSector28(u8 Channel, u8 Drive, u32 LBA, u8* Buffer);
_Bool ATAWriteSector28(u8 Channel, u8 Drive, u32 LBA, u8* Buffer);
