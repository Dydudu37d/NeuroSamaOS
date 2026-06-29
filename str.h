#pragma once
#include "int.h"

void PingPong(void** Ping,void** Pong);
void MemCopy(void* D,const void* S,size_t size);
void MemCopySize32CountByte(u32* D,const u32* S,size_t size_bytes);
void StrAdd(char* Add,const char* S,size_t size);
void MemSet(void* D,const u8 S,size_t size);
void MemSet16(void* D,const u16 S,size_t size);
void MemSet32(void* D,const u32 S,size_t size);
void MemSet64(void* D,const u64 S,size_t size);
_Bool StrIs(const char* D,const char* S);
u64 StrLen(const char* S);
void Bit64Str(u64 N,char* Buf,size_t size);
int StrSplit(char* Str, char Split, char** left, char** right);
_Bool StrIsNoCase(const char* D, const char* S);

static void __attribute__((used))memset(void* D,const u8 S,size_t size){MemSet(D,S,size);}
static void __attribute__((used))memcpy(void* D,const void* S,size_t size){MemCopy(D,S,size);}

static _Bool MemCmp(const void* S1, const void* S2, u32 N) {
    const u8* P1 = S1;
    const u8* P2 = S2;
    for (u32 I = 0; I < N; I++) {
        if (P1[I] != P2[I]) return 0;
    }
    return 1;
}
