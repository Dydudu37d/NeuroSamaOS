#pragma once
#include "int.h"

typedef struct {
    _Bool Compression : 1;
    char Char1 : 7;
    u32 *Data;
    u32 *PosMap;
    u64 Size;
    u64 OriginalSize;
}__attribute__((packed)) NeuroLossless;

NeuroLossless Compress(u32* Data,u64 Size);
u32* DeCompress(NeuroLossless Data);