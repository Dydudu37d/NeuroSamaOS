#include "kmalloc.h"
#include "int.h"

void PoolAddBlock(AllocPool *Pool, AllocBlock *Block){
    if ((!Pool) || (!Block)) return;
    Block->next=Pool->Head;
    Pool->Head=Block;
}

void* DoSplit(AllocPool *Pool, AllocBlock *curr, void* aligned_ptr, size_t size) {
    if (!Pool || !curr || !aligned_ptr || !size) return NULL;
    size_t total_size = curr->size;
    size_t used_offset = (u8*)aligned_ptr - (u8*)curr;
    size_t remaining_size = total_size - used_offset - size;

    if (remaining_size > sizeof(AllocBlock)) {
        AllocBlock *new_block = (AllocBlock*)((u8*)aligned_ptr + size);

        new_block->size = remaining_size - sizeof(AllocBlock);
        new_block->is_free = 1;
        new_block->next = curr->next;
        
        curr->size = used_offset - sizeof(AllocBlock);
        curr->next = new_block;
    }
    curr->is_free = 0; 
    return aligned_ptr;
}

void *Alloc(AllocPool *Pool, size_t size){
    if (!Pool || !size) return NULL;
    AllocBlock *curr = Pool->Head;
    while (curr) {
        while (curr->is_free&&curr->next&&curr->next->is_free){
            curr->size += sizeof(AllocBlock) + curr->next->size;
            curr->next = curr->next->next;
        }
        if (curr->is_free && curr->size>=size){
            curr->is_free = 0;
            return (void*)((u8*)curr + sizeof(AllocBlock));
        }
        
        curr=curr->next;
    }
    return NULL;
}

void *Aligned_Alloc(AllocPool *Pool, size_t size, u8 Aligned) {
    if (!Pool || !size) return NULL;
    if (!Aligned){
        return Alloc(Pool,size);
    }
    AllocBlock *curr = Pool->Head;
    
    while (curr) {
        while (curr->is_free && curr->next && curr->next->is_free) {
            curr->size += sizeof(AllocBlock) + curr->next->size;
            curr->next = curr->next->next;
        }

        if (curr->is_free && curr->size >= size) {
            u64 block_start = (u64)curr + sizeof(AllocBlock);
            u64 aligned_ptr = (block_start + (Aligned - 1)) & ~(u64)(Aligned - 1);
            size_t padding = aligned_ptr - block_start;

            if (curr->size >= size + padding) {
                return DoSplit(Pool, curr, (void*)aligned_ptr, size);
            }
        }
        curr = curr->next;
    }
    return NULL;
}

void Free(AllocPool *Pool, void *ptr) {
    if (!ptr || !Pool) return;

    AllocBlock *block = (AllocBlock*)((u8*)ptr - sizeof(AllocBlock));

    if (block->is_free) {
        return; 
    }

    block->is_free = 1;
}
