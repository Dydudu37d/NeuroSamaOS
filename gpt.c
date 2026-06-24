#include "gpt.h"
#include "ata.h"
#include "kmalloc.h"
#include "str.h"

extern AllocPool KernelPool;

u64 g_esp_start_lba = 0;
u64 g_esp_end_lba = 0;

_Bool GPTDetect(u8 Channel, u8 Drive) {
    u8 sector[512];
    
    if (!ATAReadSector(Channel, Drive, 1, sector)) {
        return 0;
    }
    
    GPTPartitionTableHeader* header = (GPTPartitionTableHeader*)sector;
    
    if (header->Signature != GPT_SIGNATURE) {
        return 0;
    }
    
    u32 table_size = header->NumberOfPartitionEntries * header->SizeOfPartitionEntry;
    u32 table_sectors = (table_size + 511) / 512;
    
    u8* table = (u8*)Alloc(&KernelPool,table_size);
    if (!table) return 0;
    
    for (u32 i = 0; i < table_sectors; i++) {
        ATAReadSector(Channel, Drive, header->PartitionEntryLBA + i, table + i * 512);
    }
    
    for (u32 i = 0; i < header->NumberOfPartitionEntries; i++) {
        GPTPartitionEntry* entry = (GPTPartitionEntry*)(table + i * header->SizeOfPartitionEntry);
        
        if (entry->StartingLBA == 0) continue;
        
        char esp_guid[16] = GPT_GUID_EFI_SYSTEM_PARTITION;
        if (StrIs(entry->PartitionTypeGUID,esp_guid)) {
            g_esp_start_lba = entry->StartingLBA;
            g_esp_end_lba = entry->EndingLBA;
            Free(&KernelPool,table);
            return 1;
        }
    }
    
    Free(&KernelPool,table);
    return 0;
}

u64 GPTGetESPStartLBA(void) {
    return g_esp_start_lba;
}

u64 GPTGetESPEndLBA(void) {
    return g_esp_end_lba;
}
