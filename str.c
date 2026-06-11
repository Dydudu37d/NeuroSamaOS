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

void MemCopy(void* D, const void* S, size_t size) {
    if (size >= 2000000 && ((u64)D & 0x1F) == 0) {
        size_t loops = size / 32;
        size_t remainder = size % 32;

        __asm__ __volatile__(
            "1:\n\t"
            "vmovdqu ymm0, [%1]\n\t"
            "vmovntdq [ %0 ], ymm0\n\t"
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

void MemSet(u8* D, const u8 S, size_t size) {
    if (size >= 2000000 && ((u64)D & 0x1F) == 0) {
        size_t loops = size / 32;

        __asm__ __volatile__(
            "movd %2, %%xmm0\n\t"
            "vpbroadcastb %%xmm0, %%ymm0\n\t"

            "1:\n\t"
            "vmovntdq %%ymm0, [%0]\n\t"
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

void Bit64Str(u64 N,char* Buf,size_t size){
    for (u64 idx=0;idx<size&&idx<65;idx++){
        Buf[idx]=(N&1ULL<<idx)?49:48;
    }
    Buf[size]='\0';
}
