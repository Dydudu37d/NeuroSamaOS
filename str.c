#include "str.h"

void PingPong(void **Ping, void **Pong){
    void* Temp=*Pong;
    *Pong=*Ping;
    *Ping=Temp;
}

void StrAdd(char* Add,const char* S,size_t size){
    u64 start=0;
    while (Add[start]!='\0') start++;

    MemCopy(Add+start,S,size);
    Add[start + size] = '\0';
}

void MemCopySize32CountByte(u32* D, const u32* S, size_t size_bytes) {
    if (size_bytes==0) return;
    if (size_bytes >= 2000000 && ((u64)D & 0x1F) == 0 && (size_bytes & 0x1F) == 0 && ((u64)S & 0x1F) == 0) {
        size_t loops = size_bytes / 32;

        __asm__ __volatile__(
            "1:\n\t"
            "vmovntdqa (%1), %%ymm0\n\t"
            "vmovntdq %%ymm0, (%0)\n\t"
            "addq $32, %0\n\t"
            "addq $32, %1\n\t"
            "decq %2\n\t"
            "jnz 1b\n\t"
            "vzeroupper\n\t"
            "sfence"
            : "+r"(D), "+r"(S), "+r"(loops)
            :: "ymm0", "memory"
        );
    } 
    else {
        size_t dwords = size_bytes / 4; 
        __asm__ __volatile__(
            "cld\n\t"
            "rep movsl"
            : "+D"(D), "+S"(S), "+c"(dwords)
            :: "memory"
        );
    }
}

void MemCopy(void* D, const void* S, size_t size) {
    if (size==0) return;
    if (size >= 2000000 && ((u64)D & 0x1F) == 0 && ((u64)S & 0x1F) == 0) {
        size_t loops = size >> 5;
        size_t remainder = size & 0x1F;

        __asm__ __volatile__(
            "1:\n\t"
            "vmovntdqa (%1), %%ymm0\n\t"
            "vmovntdq %%ymm0, (%0)\n\t"
            "addq $32, %0\n\t"
            "addq $32, %1\n\t"
            "decq %2\n\t"
            "jnz 1b\n\t"
            "vzeroupper\n\t"
            "sfence"
            : "+r"(D), "+r"(S), "+r"(loops)
            :
            : "ymm0", "memory"
        );

        if (remainder > 0) {
            __asm__ __volatile__("cld\n\t rep movsb"
                : "+D"(D), "+S"(S), "+c"(remainder)
                :: "memory");
        }
    } else {
        __asm__ __volatile__("cld\n\t rep movsb"
            : "+D"(D), "+S"(S), "+c"(size)
            :: "memory");
    }
}


void MemSet(void* D, const u8 S, size_t size) {
    if (size==0) return;
    if (size >= 1000000 && ((u64)D & 0x0F) == 0 && (size & 31) == 0) {
        size_t loops = size / 32;

        __asm__ __volatile__(
            "movd %2, %%xmm0\n\t"
            "vpbroadcastb %%xmm0, %%ymm0\n\t"

            "1:\n\t"
            "vmovntdq %%ymm0, (%0)\n\t"
            "addq $32, %0\n\t"
            "decq %1\n\t"
            "jnz 1b\n\t"
            "vzeroupper\n\t"
            "sfence"
            : "+r"(D), "+r"(loops)
            : "r"((u64)S)
            : "ymm0", "memory"
        );
    } else {
        __asm__ __volatile__("cld\n\t rep stosb"
            : "+D"(D), "+c"(size)
            : "a"(S)
            : "memory");
    }
}

void MemSet16(void* D, const u16 S, size_t size) {
    if (size==0) return;
    if (size >= 1000000 && ((u64)D & 0x0F) == 0 && (size & 31) == 0) {
        size_t loops = size / 16; 

        __asm__ __volatile__(
            "movd %2, %%xmm0\n\t"
            "vpbroadcastw %%xmm0, %%ymm0\n\t"

            "1:\n\t"
            "vmovntdq %%ymm0, (%0)\n\t"
            "addq $32, %0\n\t"
            "decq %1\n\t"
            "jnz 1b\n\t"
            "vzeroupper\n\t"
            "sfence"
            : "+r"(D), "+r"(loops)
            : "r"((u64)S)
            : "ymm0", "memory"
        );
    } else {
        __asm__ __volatile__(
            "cld\n\t rep stosw"
            : "+D"(D), "+c"(size)
            : "a"(S)
            : "memory"
        );
    }
}

void MemSet32(void* D, const u32 S, size_t size) {
    if (size==0) return;
    if (size >= 1000000 && ((u64)D & 0x0F) == 0 && (size & 31) == 0) {
        size_t loops = size / 8; 

        __asm__ __volatile__(
            "movd %2, %%xmm0\n\t"
            "vpbroadcastd %%xmm0, %%ymm0\n\t"

            "1:\n\t"
            "vmovntdq %%ymm0, (%0)\n\t"
            "addq $32, %0\n\t"
            "decq %1\n\t"
            "jnz 1b\n\t"
            "vzeroupper\n\t"
            "sfence"
            : "+r"(D), "+r"(loops)
            : "r"((u64)S)
            : "ymm0", "memory"
        );
    } else {
        __asm__ __volatile__(
            "cld\n\t rep stosl"
            : "+D"(D), "+c"(size)
            : "a"(S)
            : "memory"
        );
    }
}

void MemSet64(void* D, const u64 S, size_t size) {
    if (size==0) return;
    if (size >= 1000000 && ((u64)D & 0x0F) == 0 && (size & 31) == 0) {
        size_t loops = size / 4; 

        __asm__ __volatile__(
            "movd %2, %%xmm0\n\t"
            "vpbroadcastq %%xmm0, %%ymm0\n\t"

            "1:\n\t"
            "vmovntdq %%ymm0, (%0)\n\t"
            "addq $32, %0\n\t"
            "decq %1\n\t"
            "jnz 1b\n\t"
            "vzeroupper\n\t"
            "sfence"
            : "+r"(D), "+r"(loops)
            : "r"((u64)S)
            : "ymm0", "memory"
        );
    } else {
        __asm__ __volatile__(
            "cld\n\t rep stosq"
            : "+D"(D), "+c"(size)
            : "a"(S)
            : "memory"
        );
    }
}

void Bit64Str(u64 N,char* Buf){
    for (s64 idx=63;idx>=0;idx--){
        Buf[63 - idx] = ((N >> idx) & 1) + 48;
    }
    Buf[64]='\0';
}

_Bool StrIs(const char* D, const char* S) {
    if (!D || !S) return D == S;
    while (*D && *S) {
        if (*D != *S) return 0;
        D++;
        S++;
    }
    return *D == *S;
}

_Bool StrIsNoCase(const char* D, const char* S) {
    if (!D || !S) return D == S;
    while (*D && *S) {
        if ((*D | 0x20) != (*S | 0x20)) return 0;
        D++; S++;
    }
    return (*D | 0x20) == (*S | 0x20);
}

u64 StrLen(const char* S) {
    if (!S) return 0;
    u64 len = 0;
    while (*S) {
        len++;
        S++;
    }
    return len;
}

int StrSplit(char* Str, char Split, char** left, char** right) {
    if (!Str || !left || !right) return -1;
    
    char* pos = Str;
    while (*pos && *pos != Split) pos++;
    
    if (*pos == Split) {
        *pos = '\0';
        *left = Str;
        *right = pos + 1;
        return 0;
    }
    
    *left = Str;
    *right = NULL;
    return 0;
}