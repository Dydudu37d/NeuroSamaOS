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
void Bit64Str(u64 N,char* Buf,size_t size);

static void __attribute__((used))memset(void* D,const u8 S,size_t size){MemSet(D,S,size);}
static void __attribute__((used))memcpy(void* D,const void* S,size_t size){MemCopy(D,S,size);}

