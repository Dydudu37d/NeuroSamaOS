#include "kmalloc.h"

static void MergeFreeBlocks(AllocPool* Pool, AllocBlock* block) {
    if (!block || !block->is_free) return;
    
    AllocBlock* next = block->next;
    while (next && next->is_free) {
        block->size += sizeof(AllocBlock) + next->size;
        block->next = next->next;
        if (block->next) block->next->prev = block;
        next = block->next;
    }
    
    AllocBlock* prev = block->prev;
    if (prev && prev->is_free) {
        prev->size += sizeof(AllocBlock) + block->size;
        prev->next = block->next;
        if (block->next) block->next->prev = prev;
    }
}

void PoolAddBlock(AllocPool* Pool, AllocBlock* Block) {
    if (!Pool || !Block) return;
    Block->next = Pool->Head;
    Block->prev = NULL;
    Block->is_free = 1;
    if (Pool->Head) Pool->Head->prev = Block;
    Pool->Head = Block;
}

static void* DoSplit(AllocBlock* curr, void* aligned_ptr, size_t size) {
    if (!curr || !aligned_ptr || !size) return NULL;
    
    size_t used_offset = (u8*)aligned_ptr - (u8*)curr;
    size_t remaining_size = curr->size - used_offset - size;
    
    if (remaining_size >= sizeof(AllocBlock) + 8) {
        AllocBlock* new_block = (AllocBlock*)((u8*)aligned_ptr + size);
        new_block->size = remaining_size - sizeof(AllocBlock);
        new_block->is_free = 1;
        new_block->next = curr->next;
        new_block->prev = curr;
        if (curr->next) curr->next->prev = new_block;
        
        curr->size = used_offset - sizeof(AllocBlock);
        curr->next = new_block;
    }
    
    curr->is_free = 0;
    return aligned_ptr;
}

void* Alloc(AllocPool* Pool, size_t size) {
    if (!Pool || !size) return NULL;
    
    size = (size + 7) & ~7;
    
    AllocBlock* curr = Pool->Head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            void* result = (void*)((u8*)curr+sizeof(AllocBlock));
            return DoSplit(curr, result, size);
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
            
            u64 aligned_ptr = (block_start + 8 + align - 1) & ~(align - 1);
            size_t padding = aligned_ptr - block_start;

            if (curr->size >= size + padding) {
                void* res = DoSplit(curr, (void*)aligned_ptr, size);
                if (res) {
                    *(AllocBlock**)((u8*)res - 8) = curr;
                }
                return res;
            }
        }
        curr = curr->next;
    }
    return NULL;
}

void Free(AllocPool* Pool, void* ptr) {
    if (!ptr || !Pool) return;
    
    AllocBlock* block = *(AllocBlock**)((u8*)ptr - 8); 
    
    if (block->is_free) return;
    block->is_free = 1;
    MergeFreeBlocks(Pool, block);
}
