#pragma once
#include "int.h"

void PingPong(void** Ping,void** Pong);
void MemCopy(void* D,const void* S,size_t size);
void StrAdd(char* Add,const char* S,size_t size);
void MemSet(u8* D,const u8 S,size_t size);
void Bit64Str(u64 N,char* Buf,size_t size);

