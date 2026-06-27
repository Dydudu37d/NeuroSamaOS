#include "fat32.h"
#include "ata.h"
#include "gpt.h"
#include "str.h"

static u64 ClusterToLba(Fat32Context* Context, u32 Cluster) {
    if (Cluster < 2) {
        Cluster = Context->RootCluster;
        if (Cluster < 2) return 0;
    }
    return Context->FirstDataLba + (u64)(Cluster - 2) * Context->SectorsPerCluster;
}

static u32 GetNextCluster(Fat32Context* Context, u32 CurrentCluster) {
    if (CurrentCluster < 2 || CurrentCluster >= 0x0FFFFFF8) {
        return 0x0FFFFFFF;
    }
    
    u32 FatOffset = CurrentCluster * 4;
    u32 SectorOffset = FatOffset / 512;
    u32 EntryOffset = FatOffset % 512;
    u64 FatSectorLba = Context->FirstFatLba + SectorOffset;
    u8 SectorBuffer[512];

    if (!ATAReadSector(Context->Channel, Context->Drive, FatSectorLba, SectorBuffer)) {
        return 0x0FFFFFFF;
    }

    u32 NextCluster = *(u32*)&SectorBuffer[EntryOffset];
    return NextCluster & 0x0FFFFFFF;
}

_Bool Fat32Init(Fat32Context* Context, u8 Channel, u8 Drive) {
    u64 EspStartLba = GPTGetESPStartLBA();
    if (EspStartLba == 0) {
        return 0;
    }

    Context->Channel = Channel;
    Context->Drive = Drive;
    Context->StartLba = EspStartLba;

    u8 SectorBuffer[512];
    if (!ATAReadSector(Channel, Drive, Context->StartLba, SectorBuffer)) {
        return 0;
    }

    Fat32Dbr* Dbr = (Fat32Dbr*)SectorBuffer;
    if (Dbr->BytesPerSector != 512 || Dbr->NumFats == 0 || Dbr->FatSize32 == 0 || Dbr->SectorsPerCluster == 0) {
        return 0;
    }

    u16* Sig = (u16*)&SectorBuffer[510];
    if (*Sig != 0xAA55) {
        return 0;
    }

    Context->SectorsPerCluster = Dbr->SectorsPerCluster;
    Context->ReservedSectorCount = Dbr->ReservedSectorCount;
    Context->FatSize = Dbr->FatSize32;
    Context->RootCluster = Dbr->RootCluster;
    Context->FirstFatLba = Context->StartLba + Dbr->ReservedSectorCount;
    Context->FirstDataLba = Context->FirstFatLba + ((u64)Dbr->NumFats * Dbr->FatSize32);

    return 1;
}

static void FormatTo83(const char* Src, char* Dest) {
    for (int I = 0; I < 11; I++) Dest[I] = ' ';
    int I = 0;
    while (Src[I] != '\0' && Src[I] != '.' && I < 8) {
        char C = Src[I];
        if (C >= 'a' && C <= 'z') C -= 32;
        Dest[I] = C;
        I++;
    }
    while (Src[I] != '\0' && Src[I] != '.') I++;
    if (Src[I] == '.') {
        I++;
        int J = 0;
        while (Src[I] != '\0' && J < 3) {
            char C = Src[I];
            if (C >= 'a' && C <= 'z') C -= 32;
            Dest[8 + J] = C;
            I++;
            J++;
        }
    }
}

static void WriteFatEntry(Fat32Context* Context, u32 Cluster, u32 Value) {
    u32 FatOffset = Cluster * 4;
    u32 SectorOffset = FatOffset / 512;
    u32 EntryOffset = FatOffset % 512;
    u8 FatSector[512];
    u64 FatLba = Context->FirstFatLba + SectorOffset;

    if (!ATAReadSector(Context->Channel, Context->Drive, FatLba, FatSector)) return;
    *(u32*)&FatSector[EntryOffset] = Value;
    ATAWriteSector(Context->Channel, Context->Drive, FatLba, FatSector);

    for (u8 I = 1; I < 2; I++) {
        u64 BackupFatLba = Context->FirstFatLba + ((u64)I * Context->FatSize) + SectorOffset;
        ATAReadSector(Context->Channel, Context->Drive, BackupFatLba, FatSector);
        *(u32*)&FatSector[EntryOffset] = Value;
        ATAWriteSector(Context->Channel, Context->Drive, BackupFatLba, FatSector);
    }
}

_Bool Fat32ReadFile(Fat32Context* Context, const char* Path, u8* Buffer, u32* OutFileSize) {
    u32 CurrentCluster = Context->RootCluster;
    const char* PathPtr = Path;
    char Target83[11];
    u8 DirSector[512];

    while (*PathPtr != '\0') {
        char Component[13];
        int CIdx = 0;
        while (*PathPtr != '\0' && *PathPtr != '\\' && *PathPtr != '/' && CIdx < 12) {
            Component[CIdx++] = *PathPtr++;
        }
        Component[CIdx] = '\0';
        if (*PathPtr == '\\' || *PathPtr == '/') PathPtr++;

        FormatTo83(Component, Target83);

        _Bool Found = 0;
        Fat32DirEntry FoundEntry;
        u32 SearchCluster = CurrentCluster;

        while (SearchCluster < 0x0FFFFFF8) {
            u64 StartLba = ClusterToLba(Context, SearchCluster);
            if (StartLba == 0) return 0;

            for (u32 S = 0; S < Context->SectorsPerCluster; S++) {
                if (!ATAReadSector(Context->Channel, Context->Drive, StartLba + S, DirSector)) {
                    return 0;
                }

                Fat32DirEntry* Entries = (Fat32DirEntry*)DirSector;
                for (int E = 0; E < 512 / sizeof(Fat32DirEntry); E++) {
                    if (Entries[E].Name[0] == 0x00) {
                        goto SearchDone;
                    }
                    if ((u8)Entries[E].Name[0] == 0xE5) {
                        continue;
                    }
                    if (Entries[E].Attr == FAT32_ATTR_LONG_NAME) {
                        continue;
                    }

                    if (MemCmp(Entries[E].Name, Target83, 11)) {
                        FoundEntry = Entries[E];
                        Found = 1;
                        goto SearchDone;
                    }
                }
            }
            SearchCluster = GetNextCluster(Context, SearchCluster);
        }

    SearchDone:
        if (!Found) return 0;

        CurrentCluster = ((u32)FoundEntry.FstClusHi << 16) | FoundEntry.FstClusLo;

        if (*PathPtr == '\0') {
            if (FoundEntry.Attr & FAT32_ATTR_DIRECTORY) {
                return 0;
            }
            
            if (OutFileSize) *OutFileSize = FoundEntry.FileSize;

            u32 BytesRemaining = FoundEntry.FileSize;
            u8* OutPtr = Buffer;
            u8 FileSector[512];

            while (BytesRemaining > 0 && CurrentCluster < 0x0FFFFFF8) {
                u64 FileLba = ClusterToLba(Context, CurrentCluster);
                if (FileLba == 0) return 0;
                
                for (u32 S = 0; S < Context->SectorsPerCluster && BytesRemaining > 0; S++) {
                    if (BytesRemaining >= 512) {
                        if (!ATAReadSector(Context->Channel, Context->Drive, FileLba + S, OutPtr)) return 0;
                        OutPtr += 512;
                        BytesRemaining -= 512;
                    } else {
                        if (!ATAReadSector(Context->Channel, Context->Drive, FileLba + S, FileSector)) return 0;
                        for (u32 I = 0; I < BytesRemaining; I++) {
                            OutPtr[I] = FileSector[I];
                        }
                        BytesRemaining = 0;
                    }
                }
                CurrentCluster = GetNextCluster(Context, CurrentCluster);
            }
            return 1;
        }
    }

    return 0;
}

static u32 AllocateCluster(Fat32Context* Context, u32 LastCluster) {
    u32 TotalClusters = (Context->FatSize * 512) / 4;
    u8 FatSector[512];
    u32 CurrentSector = 0xFFFFFFFF;

    for (u32 Cluster = 2; Cluster < TotalClusters; Cluster++) {
        u32 FatOffset = Cluster * 4;
        u32 SectorOffset = FatOffset / 512;
        u32 EntryOffset = FatOffset % 512;

        if (SectorOffset != CurrentSector) {
            CurrentSector = SectorOffset;
            if (!ATAReadSector(Context->Channel, Context->Drive, Context->FirstFatLba + CurrentSector, FatSector)) {
                return 0x0FFFFFFF;
            }
        }

        u32 Entry = *(u32*)&FatSector[EntryOffset];
        if ((Entry & 0x0FFFFFFF) == 0) {
            WriteFatEntry(Context, Cluster, 0x0FFFFFFF);

            if (LastCluster >= 2 && LastCluster < 0x0FFFFFF8) {
                WriteFatEntry(Context, LastCluster, Cluster);
            }
            return Cluster;
        }
    }
    return 0x0FFFFFFF;
}

_Bool Fat32WriteFile(Fat32Context* Context, const char* Path, const u8* Buffer, u32 Size, _Bool Append) {
    u32 CurrentCluster = Context->RootCluster;
    const char* PathPtr = Path;
    char Target83[11];
    u8 DirSector[512];

    u64 DirEntryLba = 0;
    u32 DirEntryOffset = 0;
    Fat32DirEntry FoundEntry = {0};
    _Bool Found = 0;

    while (*PathPtr != '\0') {
        char Component[13];
        int CIdx = 0;
        while (*PathPtr != '\0' && *PathPtr != '\\' && *PathPtr != '/' && CIdx < 12) {
            Component[CIdx++] = *PathPtr++;
        }
        Component[CIdx] = '\0';
        if (*PathPtr == '\\' || *PathPtr == '/') PathPtr++;

        FormatTo83(Component, Target83);
        Found = 0;
        u32 SearchCluster = CurrentCluster;

        while (SearchCluster < 0x0FFFFFF8) {
            u64 StartLba = ClusterToLba(Context, SearchCluster);
            if (StartLba == 0) return 0;

            for (u32 S = 0; S < Context->SectorsPerCluster; S++) {
                if (!ATAReadSector(Context->Channel, Context->Drive, StartLba + S, DirSector)) {
                    return 0;
                }

                Fat32DirEntry* Entries = (Fat32DirEntry*)DirSector;
                for (int E = 0; E < 512 / sizeof(Fat32DirEntry); E++) {
                    if (Entries[E].Name[0] == 0x00) {
                        goto SearchDone;
                    }
                    if ((u8)Entries[E].Name[0] == 0xE5) {
                        continue;
                    }
                    if (Entries[E].Attr == FAT32_ATTR_LONG_NAME) {
                        continue;
                    }

                    if (MemCmp(Entries[E].Name, Target83, 11)) {
                        FoundEntry = Entries[E];
                        DirEntryLba = StartLba + S;
                        DirEntryOffset = E * sizeof(Fat32DirEntry);
                        Found = 1;
                        goto SearchDone;
                    }
                }
            }
            SearchCluster = GetNextCluster(Context, SearchCluster);
        }

    SearchDone:
        if (!Found) return 0;

        if (*PathPtr != '\0') {
            CurrentCluster = ((u32)FoundEntry.FstClusHi << 16) | FoundEntry.FstClusLo;
        }
    }

    if (FoundEntry.Attr & FAT32_ATTR_DIRECTORY) {
        return 0;
    }

    u32 FileStartCluster = ((u32)FoundEntry.FstClusHi << 16) | FoundEntry.FstClusLo;
    u32 LastCluster = FileStartCluster;
    u32 WriteOffset = 0;

    if (Append && FoundEntry.FileSize > 0) {
        WriteOffset = FoundEntry.FileSize;
        LastCluster = FileStartCluster;
        
        if (LastCluster >= 2 && LastCluster < 0x0FFFFFF8) {
            while (1) {
                u32 Next = GetNextCluster(Context, LastCluster);
                if (Next >= 0x0FFFFFF8) break;
                LastCluster = Next;
            }
        }
    } else {
        if (FileStartCluster >= 2 && FileStartCluster < 0x0FFFFFF8) {
            u32 PrevCluster = FileStartCluster;
            while (PrevCluster < 0x0FFFFFF8 && PrevCluster >= 2) {
                u32 Next = GetNextCluster(Context, PrevCluster);
                WriteFatEntry(Context, PrevCluster, 0);
                PrevCluster = Next;
            }
        }
        FileStartCluster = 0;
        LastCluster = 0;
        FoundEntry.FileSize = 0;
    }

    u32 BytesLeft = Size;
    const u8* InPtr = Buffer;
    u32 ClusterSize = Context->SectorsPerCluster * 512;
    u8 WriteSector[512];
    u32 TotalWritten = WriteOffset;

    while (BytesLeft > 0) {
        u32 ClusterOffset = TotalWritten % ClusterSize;
        u32 SectorInCluster = ClusterOffset / 512;
        u32 OffsetInSector = ClusterOffset % 512;

        if (ClusterOffset == 0 || FileStartCluster == 0) {
            u32 NewCluster = AllocateCluster(Context, LastCluster);
            if (NewCluster >= 0x0FFFFFF8) return 0;
            if (FileStartCluster == 0) {
                FileStartCluster = NewCluster;
                FoundEntry.FstClusHi = (u16)(FileStartCluster >> 16);
                FoundEntry.FstClusLo = (u16)(FileStartCluster & 0xFFFF);
            }
            LastCluster = NewCluster;
            SectorInCluster = 0;
            OffsetInSector = 0;
        }

        u64 TargetLba = ClusterToLba(Context, LastCluster) + SectorInCluster;
        if (TargetLba == 0) return 0;
        
        if (OffsetInSector != 0 || BytesLeft < 512) {
            if (!ATAReadSector(Context->Channel, Context->Drive, TargetLba, WriteSector)) return 0;
        }

        u32 Chunk = 512 - OffsetInSector;
        if (Chunk > BytesLeft) Chunk = BytesLeft;

        for (u32 I = 0; I < Chunk; I++) {
            WriteSector[OffsetInSector + I] = InPtr[I];
        }

        if (!ATAWriteSector(Context->Channel, Context->Drive, TargetLba, WriteSector)) return 0;

        InPtr += Chunk;
        BytesLeft -= Chunk;
        TotalWritten += Chunk;
    }

    FoundEntry.FileSize = TotalWritten;

    if (!ATAReadSector(Context->Channel, Context->Drive, DirEntryLba, DirSector)) return 0;
    Fat32DirEntry* TargetEntry = (Fat32DirEntry*)&DirSector[DirEntryOffset];
    *TargetEntry = FoundEntry;
    if (!ATAWriteSector(Context->Channel, Context->Drive, DirEntryLba, DirSector)) return 0;

    return 1;
}

_Bool Fat32SetAttribute(Fat32Context* Context, const char* Path, u8 Attribute) {
    u32 CurrentCluster = Context->RootCluster;
    const char* PathPtr = Path;
    char Target83[11];
    u8 DirSector[512];

    u64 DirEntryLba = 0;
    u32 DirEntryOffset = 0;
    Fat32DirEntry FoundEntry;
    _Bool Found = 0;

    while (*PathPtr != '\0') {
        char Component[13];
        int CIdx = 0;
        while (*PathPtr != '\0' && *PathPtr != '\\' && *PathPtr != '/' && CIdx < 12) {
            Component[CIdx++] = *PathPtr++;
        }
        Component[CIdx] = '\0';
        if (*PathPtr == '\\' || *PathPtr == '/') PathPtr++;

        FormatTo83(Component, Target83);
        Found = 0;
        u32 SearchCluster = CurrentCluster;

        while (SearchCluster < 0x0FFFFFF8) {
            u64 StartLba = ClusterToLba(Context, SearchCluster);
            if (StartLba == 0) return 0;

            for (u32 S = 0; S < Context->SectorsPerCluster; S++) {
                if (!ATAReadSector(Context->Channel, Context->Drive, StartLba + S, DirSector)) {
                    return 0;
                }

                Fat32DirEntry* Entries = (Fat32DirEntry*)DirSector;
                for (int E = 0; E < 512 / sizeof(Fat32DirEntry); E++) {
                    if (Entries[E].Name[0] == 0x00) {
                        goto SearchDone;
                    }
                    if ((u8)Entries[E].Name[0] == 0xE5) {
                        continue;
                    }
                    if (Entries[E].Attr == FAT32_ATTR_LONG_NAME) {
                        continue;
                    }

                    if (MemCmp(Entries[E].Name, Target83, 11)) {
                        FoundEntry = Entries[E];
                        DirEntryLba = StartLba + S;
                        DirEntryOffset = E * sizeof(Fat32DirEntry);
                        Found = 1;
                        goto SearchDone;
                    }
                }
            }
            SearchCluster = GetNextCluster(Context, SearchCluster);
        }

    SearchDone:
        if (!Found) return 0;

        if (*PathPtr != '\0') {
            CurrentCluster = ((u32)FoundEntry.FstClusHi << 16) | FoundEntry.FstClusLo;
        }
    }

    if (!ATAReadSector(Context->Channel, Context->Drive, DirEntryLba, DirSector)) {
        return 0;
    }

    Fat32DirEntry* TargetEntry = (Fat32DirEntry*)&DirSector[DirEntryOffset];
    TargetEntry->Attr = Attribute;

    if (!ATAWriteSector(Context->Channel, Context->Drive, DirEntryLba, DirSector)) {
        return 0;
    }

    return 1;
}