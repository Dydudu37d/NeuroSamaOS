#pragma once

#include "int.h"

#define TypeChar (1<<0)
#define TypeUnsigned (1<<1)
#define TypeSigned (1<<2)
#define TypeLong (1<<3)
#define TypeLongLong (1<<4)
#define TypeShort (1<<5)
#define TypePointer (1<<6)
#define TypeVoid (1<<7)
#define VarUnUse 0

typedef struct{
    u64 SerialNumber;
    u8 Type;
    void* DataPoint;
}__attribute__((packed)) JITVar;

typedef struct{
    u64 SerialNumber;
    u8 Type;
    void* FuncPoint;
}__attribute__((packed)) JITFunc;

u64 GetVarCount();
void SetVarDataPoint(u64 SerialNumber,u64* DataPoint);
void* GetVarDataPoint(u64 SerialNumber);
u8 GetVarType(u64 SerialNumber);
void SetVarType(u64 SerialNumber, u8 Type);
u64 VarAdd(JITVar Var);
void VarDel(u64 SerialNumber);

u64 GetFuncCount();
void SetFuncPoint(u64 SerialNumber,u64* FuncPoint);
void* GetFuncPoint(u64 SerialNumber);
u8 GetFuncType(u64 SerialNumber);
void SetFuncType(u64 SerialNumber, u8 Type);
u64 FuncAdd(JITFunc Func);
void FuncDel(u64 SerialNumber);

static inline void EmitJmp(u8 **p, void *target, void *here) {
    *(*p)++ = 0xE9;
    s32 off = (s32)((u64)target - ((u64)here + 5));
    *(s32*)(*p) = off;
    *p += 4;
}

static inline void EmitCallRax(u8 **p) {
    *(*p)++ = 0xFF;
    *(*p)++ = 0xD0;
}

static inline void EmitCall(u8 **p, void *target, void *here) {
    *(*p)++ = 0xE8;
    s32 off = (s32)((u64)target - ((u64)here + 5));
    *(s32*)(*p) = off;
    *p += 4;
}

static inline void EmitMovRax(u8 **p, u64 val) {
    *(*p)++ = 0x48;
    *(*p)++ = 0xB8;
    *(u64*)(*p) = val;
    *p += 8;
}

static inline void EmitJcc(u8 **p, u8 op1, u8 op2, void *target, void *here) {
    *(*p)++ = op1;
    *(*p)++ = op2;
    s32 off = (s32)((u64)target - ((u64)here + 6));
    *(s32*)(*p) = off;
    *p += 4;
}
