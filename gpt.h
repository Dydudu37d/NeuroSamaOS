#pragma once

#include "int.h"

#define GPT_SIGNATURE 0x5452415020494645ULL

typedef struct {
    u64 Signature;
    u32 Revision;
    u32 HeaderSize;
    u32 HeaderCRC32;
    u32 Reserved;
    u64 MyLBA;
    u64 AlternateLBA;
    u64 FirstUsableLBA;
    u64 LastUsableLBA;
    u8  DiskGUID[16];
    u64 PartitionEntryLBA;
    u32 NumberOfPartitionEntries;
    u32 SizeOfPartitionEntry;
    u32 PartitionEntryArrayCRC32;
    u8  Reserved2[420];
} __attribute__((packed)) GPTPartitionTableHeader;

typedef struct {
    char  PartitionTypeGUID[16];
    u8  UniquePartitionGUID[16];
    u64 StartingLBA;
    u64 EndingLBA;
    u64 Attributes;
    u8  PartitionName[72];
} __attribute__((packed)) GPTPartitionEntry;

#define GPT_GUID_EFI_SYSTEM_PARTITION \
    {0xC1, 0x2A, 0x73, 0x28, 0xF8, 0x1F, 0x11, 0xD2, \
     0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}

_Bool GPTDetect(u8 Channel, u8 Drive);
u64 GPTGetESPStartLBA(void);
u64 GPTGetESPEndLBA(void);

extern u64 g_esp_start_lba;
extern u64 g_esp_end_lba;
