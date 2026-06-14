#pragma once

#include "int.h"

typedef struct AllocBlock {
    struct AllocBlock* next;
    struct AllocBlock* prev;
    size_t size;
    u8 is_free;
} __attribute__((aligned(64))) AllocBlock;

typedef struct {
    AllocBlock* Head;
} __attribute__((aligned(64))) AllocPool;

void PoolAddBlock(AllocPool* Pool, AllocBlock* Block);
void* Alloc(AllocPool* Pool, size_t size);
void* AlignedAlloc(AllocPool* Pool, size_t size, size_t align);
void Free(AllocPool* Pool, void* ptr);
