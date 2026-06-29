// gpt.h
#pragma once
#include "int.h"

#define GPT_SIGNATURE 0x5452415020494645ULL
#define GPT_GUID_EFI_SYSTEM_PARTITION \
    {0x28,0x73,0x2A,0xC1,0x1F,0xF8,0xD2,0x11,0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B}

#define MAX_ESP_PARTITIONS 8

typedef struct {
    u64 Signature;
    u32 Revision;
    u32 HeaderSize;
    u32 HeaderCRC32;
    u32 Reserved;
    u64 CurrentLBA;
    u64 BackupLBA;
    u64 FirstUsableLBA;
    u64 LastUsableLBA;
    u8 DiskGUID[16];
    u64 PartitionEntryLBA;
    u32 NumberOfPartitionEntries;
    u32 SizeOfPartitionEntry;
    u32 PartitionEntryCRC32;
} __attribute__((packed)) GPTPartitionTableHeader;

typedef struct {
    u8 PartitionTypeGUID[16];
    u8 UniquePartitionGUID[16];
    u64 StartingLBA;
    u64 EndingLBA;
    u64 Attributes;
    char PartitionName[72];
} __attribute__((packed)) GPTPartitionEntry;

typedef struct {
    u64 StartLba;
    u64 EndLba;
    u8 Channel;
    u8 Drive;
    _Bool Valid;
} ESPEntry;

_Bool GPTDetect(u8 Channel, u8 Drive);
u64 GPTGetESPStartLBA(void);
u64 GPTGetESPEndLBA(void);
ESPEntry* GPTGetESP(u32 Index);
u32 GPTGetESPCount(void);