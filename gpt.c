#include "gpt.h"
#include "ata.h"
#include "kmalloc.h"
#include "str.h"
#include "debug.h"

extern AllocPool KernelPool;

#define MAX_ESP_PARTITIONS 8

static ESPEntry g_esp_partitions[MAX_ESP_PARTITIONS] = {0};
static u32 g_esp_count = 0;

_Bool GPTDetect(u8 Channel, u8 Drive) {
    DebugStr("GPTDetect: Entry\n");
    
    u8 sector[512];
    DebugStr("GPTDetect: Reading LBA1...\n");
    if (!ATAReadSector(Channel, Drive, 1, sector)) {
        DebugStr("GPTDetect: Read failed\n");
        return 0;
    }
    DebugStr("GPTDetect: Read OK\n");
    
    GPTPartitionTableHeader* header = (GPTPartitionTableHeader*)sector;
    DebugStr("GPTDetect: Signature check\n");
    
    if (header->Signature != GPT_SIGNATURE) {
        DebugStr("GPTDetect: Not GPT, returning 0\n");
        return 0;
    }
    DebugStr("GPTDetect: Is GPT\n");
    
    u64 table_size_64 = (u64)header->NumberOfPartitionEntries * header->SizeOfPartitionEntry;
    DebugStr("GPTDetect: table_size=");
    DebugU64(table_size_64);
    DebugChar('\n');
    
    if (table_size_64 > 1024 * 1024) {
        DebugStr("GPTDetect: Table too large\n");
        return 0;
    }
    u32 table_size = (u32)table_size_64;
    u32 table_sectors = (table_size + 511) / 512;
    
    DebugStr("GPTDetect: Allocating table...\n");
    u8* table = (u8*)Alloc(&KernelPool, table_size);
    if (!table) {
        DebugStr("GPTDetect: Alloc failed\n");
        return 0;
    }
    DebugStr("GPTDetect: Alloc OK\n");
    
    DebugStr("GPTDetect: Reading partition table...\n");
    for (u32 i = 0; i < table_sectors; i++) {
        if (!ATAReadSector(Channel, Drive, header->PartitionEntryLBA + i, table + i * 512)) {
            DebugStr("GPTDetect: Failed to read partition table sector\n");
            Free(&KernelPool, table);
            return 0;
        }
    }
    DebugStr("GPTDetect: Partition table read OK\n");
    
    DebugStr("GPTDetect: Scanning entries...\n");
    u8 esp_guid[16] = GPT_GUID_EFI_SYSTEM_PARTITION;
    _Bool found = 0;
    
    for (u32 i = 0; i < header->NumberOfPartitionEntries; i++) {
        GPTPartitionEntry* entry = (GPTPartitionEntry*)(table + i * header->SizeOfPartitionEntry);
        
        if (entry->StartingLBA == 0) continue;
        
        DebugStr("GPTDetect: Entry ");
        DebugU32(i);
        DebugStr(" StartingLBA=");
        DebugU64(entry->StartingLBA);
        DebugChar('\n');
        
        if (MemCmp(entry->PartitionTypeGUID, esp_guid, 16)) {
            DebugStr("GPTDetect: Found ESP!\n");
            
            if (g_esp_count < MAX_ESP_PARTITIONS) {
                g_esp_partitions[g_esp_count].StartLba = entry->StartingLBA;
                g_esp_partitions[g_esp_count].EndLba = entry->EndingLBA;
                g_esp_partitions[g_esp_count].Channel = Channel;
                g_esp_partitions[g_esp_count].Drive = Drive;
                g_esp_partitions[g_esp_count].Valid = 1;
                g_esp_count++;
                found = 1;
            } else {
                DebugStr("GPTDetect: ESP list full!\n");
                break;
            }
        }
    }
    
    DebugStr("GPTDetect: No ESP found\n");
    Free(&KernelPool, table);
    return found;
}

u64 GPTGetESPStartLBA(void) {
    for (u32 i = 0; i < g_esp_count; i++) {
        if (g_esp_partitions[i].Valid) {
            return g_esp_partitions[i].StartLba;
        }
    }
    return 0;
}

u64 GPTGetESPEndLBA(void) {
    for (u32 i = 0; i < g_esp_count; i++) {
        if (g_esp_partitions[i].Valid) {
            return g_esp_partitions[i].EndLba;
        }
    }
    return 0;
}

ESPEntry* GPTGetESP(u32 Index) {
    if (Index >= g_esp_count) return NULL;
    if (!g_esp_partitions[Index].Valid) return NULL;
    return &g_esp_partitions[Index];
}

u32 GPTGetESPCount(void) {
    return g_esp_count;
}