#include "kmalloc.h"
#include "debug.h"

static void* DoSplit(AllocPool* Pool, AllocBlock* curr, void* aligned_ptr, size_t size, int is_aligned) {
    if (!Pool || !curr || !aligned_ptr || size == 0) return NULL;
    
    u64 block_start = (u64)curr + sizeof(AllocBlock);
    u64 data_start = (u64)aligned_ptr;
    
    if (data_start > block_start) {
        size_t padding = data_start - block_start;
        AllocBlock* prefix = curr;
        
        curr = (AllocBlock*)(data_start - sizeof(AllocBlock));
        curr->size = prefix->size - padding;
        curr->is_free = 1;
        curr->is_aligned = 0;
        
        prefix->size = padding - sizeof(AllocBlock);
        
        curr->next = prefix->next;
        curr->prev = prefix;
        if (prefix->next) prefix->next->prev = curr;
        prefix->next = curr;
    }
    
    size_t remaining = curr->size - size;
    if (remaining >= sizeof(AllocBlock) + 8) {
        AllocBlock* new_block = (AllocBlock*)(data_start + size);
        new_block->size = remaining - sizeof(AllocBlock);
        new_block->is_free = 1;
        new_block->is_aligned = 0;
        
        new_block->next = curr->next;
        new_block->prev = curr;
        if (curr->next) curr->next->prev = new_block;
        
        curr->size = size;
        curr->next = new_block;
    }
    
    curr->is_free = 0;
    curr->is_aligned = is_aligned;
    return (void*)data_start;
}

void* Alloc(AllocPool* Pool, size_t size) {
    if (!Pool || !size) return NULL;
    size = (size + 7) & ~7;
    
    AllocBlock* curr = Pool->Head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            void* result = (void*)((u8*)curr + sizeof(AllocBlock));
            return DoSplit(Pool, curr, result, size, 0);
        }
        curr = curr->next;
    }
    return NULL;
}

void* AlignedAlloc(AllocPool* Pool, size_t size, size_t align) {
    if (!Pool || !size) return NULL;
    if (align < 8) align = 8;
    size = (size + 7) & ~7;

    AllocBlock* curr = Pool->Head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            u64 block_start = (u64)curr + sizeof(AllocBlock);
            u64 aligned_ptr = (block_start + align - 1) & ~(align - 1);
            
            while (aligned_ptr > block_start && (aligned_ptr - block_start) < (sizeof(AllocBlock) + 8)) {
                aligned_ptr += align;
            }
            
            size_t total_needed = (aligned_ptr - block_start) + size;
            if (curr->size >= total_needed) {
                return DoSplit(Pool, curr, (void*)aligned_ptr, size, 1);
            }
        }
        curr = curr->next;
    }
    return NULL;
}

void MergeFreeBlocks(AllocPool* Pool, AllocBlock* block) {
    if (!block || !block->is_free || (u64)block < (4 << 10)) return;
    
    DebugStr("AllocBlock* next = block->next; And Loop\n");
    AllocBlock* next = block->next;
    while (next > (AllocBlock*)0xFFF && next->is_free) {
        block->size += sizeof(AllocBlock) + next->size;
        block->next = next->next;
        if (block->next > (AllocBlock*)0xFFF) block->next->prev = block;
        next = block->next;
    }
    
    DebugStr("AllocBlock* prev = block->prev; And Loop\n");
    AllocBlock* prev = block->prev;
    if (prev > (AllocBlock*)0xFFF && prev->is_free) {
        prev->size += sizeof(AllocBlock) + block->size;
        prev->next = block->next;
        if (block->next > (AllocBlock*)0xFFF) block->next->prev = prev;
    }
}

void* MaxAlloc(AllocPool* Pool, size_t size, u64 MaxPos) {
    if (!Pool || !size) return NULL;
    size = (size + 7) & ~7;
    AllocBlock* curr = Pool->Head;
    AllocBlock* best = NULL;
    u64 best_addr = ~0ULL;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            u64 block_start = (u64)curr + sizeof(AllocBlock);
            if (block_start <= MaxPos && block_start + size <= MaxPos) {
                if (block_start < best_addr) {
                    best_addr = block_start;
                    best = curr;
                }
            }
        }
        curr = curr->next;
    }
    if (best) {
        void* result = (void*)((u8*)best + sizeof(AllocBlock));
        return DoSplit(Pool, best, result, size, 0);
    }
    return NULL;
}

void* MaxAlignedAlloc(AllocPool* Pool, size_t size, size_t align, u64 MaxPos) {
    if (!Pool || !size) return NULL;
    if (align < 8) align = 8;
    size = (size + 7) & ~7;
    AllocBlock* curr = Pool->Head;
    AllocBlock* best = NULL;
    u64 best_aligned_ptr = 0;
    u64 best_addr = ~0ULL;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            u64 block_start = (u64)curr + sizeof(AllocBlock);
            u64 aligned_ptr = (block_start + align - 1) & ~(align - 1);
            
            while (aligned_ptr > block_start && (aligned_ptr - block_start) < (sizeof(AllocBlock) + 8)) {
                aligned_ptr += align;
            }
            
            size_t total_needed = (aligned_ptr - block_start) + size;
            if (curr->size >= total_needed && aligned_ptr <= MaxPos && aligned_ptr + size <= MaxPos) {
                if (aligned_ptr < best_addr) {
                    best_addr = aligned_ptr;
                    best = curr;
                    best_aligned_ptr = aligned_ptr;
                }
            }
        }
        curr = curr->next;
    }
    if (best) {
        return DoSplit(Pool, best, (void*)best_aligned_ptr, size, 1);
    }
    return NULL;
}

void Free(AllocPool* Pool, void* ptr) {
    if (!ptr || !Pool || ptr < (4 << 10)) return;
    DebugStr("AllocPool at 0x");
    DebugU64((u64)Pool);
    DebugChar('\n');
    DebugStr("AllocPool.Block at 0x");
    DebugU64((u64)Pool->Head);
    DebugChar('\n');
    
    AllocBlock* block = (AllocBlock*)((u8*)ptr - sizeof(AllocBlock));
    
    if (block->is_aligned) {
        DebugStr("Freeing aligned block at 0x");
        DebugU64((u64)block);
        DebugStr("\n");
    } else {
        DebugStr("block at 0x");
        DebugU64((u64)block);
        DebugStr("\n");
    }
    DebugStr("if (block->is_free) return;\n");
    if (block->is_free) return;
    
    DebugStr("Set block->is_free = 1\n");
    block->is_free = 1;
    DebugStr("Set block->is_aligned = 0\n");
    block->is_aligned = 0;
    
    DebugStr("MergeFreeBlocks(Pool, block);\n");
    MergeFreeBlocks(Pool, block);
}

void PoolAddBlock(AllocPool* Pool, AllocBlock* Block) {
    if (!Pool || !Block) return;
    Block->next = Pool->Head;
    Block->prev = NULL;
    Block->is_free = 1;
    Block->is_aligned = 0;
    if (Pool->Head) Pool->Head->prev = Block;
    Pool->Head = Block;
}