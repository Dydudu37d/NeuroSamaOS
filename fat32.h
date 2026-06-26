#pragma once

#include "int.h"

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LONG_NAME 0x0F

typedef struct {
    u8  JumpBoot[3];
    char OemName[8];
    u16 BytesPerSector;
    u8  SectorsPerCluster;
    u16 ReservedSectorCount;
    u8  NumFats;
    u16 RootEntryCount;
    u16 TotalSectors16;
    u8  Media;
    u16 FatSize16;
    u16 SectorsPerTrack;
    u16 NumHeads;
    u32 HiddenSectors;
    u32 TotalSectors32;
    u32 FatSize32;
    u16 ExtFlags;
    u16 FsVersion;
    u32 RootCluster;
    u16 FsInfo;
    u16 BkBootSec;
    u8  Reserved[12];
    u8  DrvNum;
    u8  Reserved1;
    u8  BootSig;
    u32 VolumeId;
    char VolumeLabel[11];
    char FileSystemType[8];
} __attribute__((packed)) Fat32Dbr;

typedef struct {
    char Name[11];
    u8  Attr;
    u8  NtRes;
    u8  CrtTimeTenth;
    u16 CrtTime;
    u16 CrtDate;
    u16 LstAccDate;
    u16 FstClusHi;
    u16 WrtTime;
    u16 WrtDate;
    u16 FstClusLo;
    u32 FileSize;
} __attribute__((packed)) Fat32DirEntry;

typedef struct {
    u8  Channel;
    u8  Drive;
    u64 StartLba;
    u32 SectorsPerCluster;
    u32 ReservedSectorCount;
    u32 FatSize;
    u32 RootCluster;
    u64 FirstFatLba;
    u64 FirstDataLba;
} Fat32Context;

_Bool Fat32Init(u8 Channel, u8 Drive);
_Bool Fat32ReadFile(const char* Path, u8* Buffer, u32* OutFileSize);
_Bool Fat32WriteFile(const char* Path, const u8* Buffer, u32 Size, _Bool Append);
_Bool Fat32SetAttribute(const char* Path, u8 Attribute);
