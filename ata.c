#include "ata.h"
#include "port.h"
#include "debug.h"

ATADriveInfo ATADrives[2][2] = {0};

static inline void ATAWaitBSY(u16 io_base) {
    while (inb(io_base + ATA_REG_STATUS) & ATA_STATUS_BSY)
        asm volatile("pause");
}

static inline void ATAWaitDRQ(u16 io_base) {
    while (!(inb(io_base + ATA_REG_STATUS) & ATA_STATUS_DRQ))
        asm volatile("pause");
}

static inline u8 ATAReadStatus(u16 io_base) {
    return inb(io_base + ATA_REG_STATUS);
}

_Bool ATAReadSector28(u8 Channel, u8 Drive, u32 LBA, u8* Buffer) {
    u16 io_base = (Channel == 0) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
    
    ATAWaitBSY(io_base);
    outb(io_base + ATA_REG_DEVICE, 0xE0 | (Drive << 4) | ((LBA >> 24) & 0x0F));
    ATAWaitBSY(io_base);
    outb(io_base + ATA_REG_SECTOR_CNT, 1);
    outb(io_base + ATA_REG_LBA_LOW, LBA & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (LBA >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (LBA >> 16) & 0xFF);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    ATAWaitDRQ(io_base);
    
    for (int i = 0; i < 256; i++) {
        ((u16*)Buffer)[i] = inw(io_base + ATA_REG_DATA);
    }
    
    u8 status = ATAReadStatus(io_base);
    if (status & ATA_STATUS_ERR) {
        DebugStr("ATA read error: status=");
        DebugU8(status);
        DebugChar('\n');
        return 0;
    }
    return 1;
}

_Bool ATAWriteSector28(u8 Channel, u8 Drive, u32 LBA, u8* Buffer) {
    u16 io_base = (Channel == 0) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
    
    ATAWaitBSY(io_base);
    outb(io_base + ATA_REG_DEVICE, 0xE0 | (Drive << 4) | ((LBA >> 24) & 0x0F));
    ATAWaitBSY(io_base);
    outb(io_base + ATA_REG_SECTOR_CNT, 1);
    outb(io_base + ATA_REG_LBA_LOW, LBA & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (LBA >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (LBA >> 16) & 0xFF);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    ATAWaitDRQ(io_base);
    
    for (int i = 0; i < 256; i++) {
        outw(io_base + ATA_REG_DATA, ((u16*)Buffer)[i]);
    }
    
    ATAWaitBSY(io_base);
    u8 status = ATAReadStatus(io_base);
    return !(status & ATA_STATUS_ERR);
}

_Bool ATAReadSector48(u8 Channel, u8 Drive, u64 LBA, u8* Buffer) {
    u16 io_base = (Channel == 0) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
    
    DebugStr("ATAReadSector48: LBA=");
    DebugU64(LBA);
    DebugChar('\n');
    
    ATAWaitBSY(io_base);
    DebugStr("ATAReadSector48: BSY cleared\n");
    
    outb(io_base + ATA_REG_DEVICE, 0x40 | (Drive << 4));
    ATAWaitBSY(io_base);
    DebugStr("ATAReadSector48: DEVICE set\n");
    
    outb(io_base + ATA_REG_SECTOR_CNT, 1);
    outb(io_base + ATA_REG_LBA_LOW, LBA & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (LBA >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (LBA >> 16) & 0xFF);
    
    outb(io_base + ATA_REG_SECTOR_CNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, (LBA >> 24) & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (LBA >> 32) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (LBA >> 40) & 0xFF);
    DebugStr("ATAReadSector48: LBA registers set\n");
    
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);
    DebugStr("ATAReadSector48: Command sent, waiting DRQ...\n");
    
    ATAWaitDRQ(io_base);
    DebugStr("ATAReadSector48: DRQ ready\n");
    
    for (int i = 0; i < 256; i++) {
        ((u16*)Buffer)[i] = inw(io_base + ATA_REG_DATA);
    }
    DebugStr("ATAReadSector48: Data read\n");
    
    u8 status = ATAReadStatus(io_base);
    if (status & ATA_STATUS_ERR) {
        DebugStr("ATA read48 error: status=");
        DebugU8(status);
        DebugChar('\n');
        return 0;
    }
    return 1;
}

_Bool ATAWriteSector48(u8 Channel, u8 Drive, u64 LBA, u8* Buffer) {
    u16 io_base = (Channel == 0) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
    
    ATAWaitBSY(io_base);
    outb(io_base + ATA_REG_DEVICE, 0x40 | (Drive << 4));
    ATAWaitBSY(io_base);
    
    outb(io_base + ATA_REG_SECTOR_CNT, 1);
    outb(io_base + ATA_REG_LBA_LOW, LBA & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (LBA >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (LBA >> 16) & 0xFF);
    
    outb(io_base + ATA_REG_SECTOR_CNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, (LBA >> 24) & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (LBA >> 32) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (LBA >> 40) & 0xFF);
    
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO_EXT);
    ATAWaitDRQ(io_base);
    
    for (int i = 0; i < 256; i++) {
        outw(io_base + ATA_REG_DATA, ((u16*)Buffer)[i]);
    }
    
    ATAWaitBSY(io_base);
    u8 status = ATAReadStatus(io_base);
    if (status & ATA_STATUS_ERR) {
        DebugStr("ATA write48 error: status=");
        DebugU8(status);
        DebugChar('\n');
        return 0;
    }
    return 1;
}

_Bool ATADetectDrive(u8 Channel, u8 Drive) {
    u16 io_base = (Channel == 0) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
    
    ATAWaitBSY(io_base);
    outb(io_base + ATA_REG_DEVICE, 0xE0 | (Drive << 4));
    ATAWaitBSY(io_base);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    u8 status = ATAReadStatus(io_base);
    if (status == 0) {
        ATADrives[Channel][Drive].Present = 0;
        return 0;
    }
    
    while (1) {
        status = ATAReadStatus(io_base);
        if (status & ATA_STATUS_ERR) {
            ATADrives[Channel][Drive].Present = 0;
            return 0;
        }
        if (status & ATA_STATUS_DRQ) break;
        asm volatile("pause");
    }
    
    u16 buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(io_base + ATA_REG_DATA);
    }
    
    ATADrives[Channel][Drive].Present = 1;
    
    if (buffer[83] & (1 << 10)) {
        ATADrives[Channel][Drive].SupportsLBA48 = 1;
        ATADrives[Channel][Drive].TotalSectors = 
            ((u64)buffer[103] << 48) |
            ((u64)buffer[102] << 32) |
            ((u64)buffer[101] << 16) |
            buffer[100];
        ATADrives[Channel][Drive].MaxLBA28 = 0x0FFFFFFF;
    } else {
        ATADrives[Channel][Drive].SupportsLBA48 = 0;
        ATADrives[Channel][Drive].TotalSectors = 
            ((u32)buffer[61] << 16) | buffer[60];
        ATADrives[Channel][Drive].MaxLBA28 = 
            (ATADrives[Channel][Drive].TotalSectors > 0x0FFFFFFF) ? 
            0x0FFFFFFF : (u32)ATADrives[Channel][Drive].TotalSectors;
    }
    
    DebugStr("ATA drive detected: Channel ");
    DebugU8(Channel);
    DebugStr(", Drive ");
    DebugU8(Drive);
    DebugStr(", LBA48=");
    DebugU8(ATADrives[Channel][Drive].SupportsLBA48);
    DebugStr(", Sectors=");
    DebugU64(ATADrives[Channel][Drive].TotalSectors);
    DebugChar('\n');
    
    return 1;
}

_Bool ATAReadSector(u8 Channel, u8 Drive, u64 LBA, u8* Buffer) {
    if (!ATADrives[Channel][Drive].Present) return 0;
    
    if (LBA > 0x0FFFFFFF) {
        return ATAReadSector48(Channel, Drive, LBA, Buffer);
    } else {
        return ATAReadSector28(Channel, Drive, (u32)LBA, Buffer);
    }
}

_Bool ATAWriteSector(u8 Channel, u8 Drive, u64 LBA, u8* Buffer) {
    if (!ATADrives[Channel][Drive].Present) return 0;
    
    if (LBA > 0x0FFFFFFF) {
        return ATAWriteSector48(Channel, Drive, LBA, Buffer);
    } else {
        return ATAWriteSector28(Channel, Drive, (u32)LBA, Buffer);
    }
}

_Bool ATAInit(void) {
    DebugStr("Initializing ATA...\n");
    ATADetectDrive(0, 0);
    ATADetectDrive(0, 1);
    ATADetectDrive(1, 0);
    ATADetectDrive(1, 1);
    DebugStr("Init ATA Finish...\n");
    return 1;
}