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
#define TypeList (1<<8)
#define VarUnUse 0

typedef struct{
    u64 SerialNumber;
    u16 Type;
    u64 size;
    void* DataPoint;
}__attribute__((packed)) JITVar;

typedef struct{
    u64 SerialNumber;
    u16 Type;
    void* FuncPoint;
}__attribute__((packed)) JITFunc;

u64 GetVarCount();
JITVar GetVar(u64 SerialNumber);
void SetVarDataPoint(u64 SerialNumber,u64* DataPoint);
void* GetVarDataPoint(u64 SerialNumber);
u8 GetVarType(u64 SerialNumber);
void SetVarType(u64 SerialNumber, u8 Type);
u64 VarAdd(JITVar Var);
void VarDel(u64 SerialNumber);

u64 GetFuncCount();
JITFunc GetFunc(u64 SerialNumber);
void SetFuncPoint(u64 SerialNumber,u64* FuncPoint);
void* GetFuncPoint(u64 SerialNumber);
u8 GetFuncType(u64 SerialNumber);
void SetFuncType(u64 SerialNumber, u8 Type);
u64 FuncAdd(JITFunc Func);
void FuncDel(u64 SerialNumber);

extern __attribute__((aligned(64))) JITVar Vars[1<<20];
extern __attribute__((aligned(64))) JITFunc Funcs[1<<20];

extern u64 VarCount;
extern u64 VarEnd;
extern u64 FuncCount;
extern u64 FuncEnd;


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

static inline void EmitPushRax(u8 **p) {
    *(*p)++ = 0x50;
}

static inline void EmitPopRbx(u8 **p) {
    *(*p)++ = 0x5B;
}

static inline void EmitAddRaxRbx(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x01;
    *(*p)++ = 0xD8;
}

static inline void EmitSubRaxRbx(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x29;
    *(*p)++ = 0xD8;
}

static inline void EmitMulRaxRbx(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0xF7;
    *(*p)++ = 0xEB;
}

static inline void EmitDivRaxRbx(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0xF7;
    *(*p)++ = 0xF3;
}

static inline void EmitModRaxRbx(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0xF7;
    *(*p)++ = 0xF3;
    *(*p)++ = 0x48;
    *(*p)++ = 0x89;
    *(*p)++ = 0xD0;
}

static inline void EmitCmpRaxRbx(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x39;
    *(*p)++ = 0xD8;
}

static inline void EmitSetzRax(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x0F;
    *(*p)++ = 0x94;
    *(*p)++ = 0xC0;
}

static inline void EmitSetnzRax(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x0F;
    *(*p)++ = 0x95;
    *(*p)++ = 0xC0;
}

static inline void EmitSetlRax(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x0F;
    *(*p)++ = 0x9C;
    *(*p)++ = 0xC0;
}

static inline void EmitSetleRax(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x0F;
    *(*p)++ = 0x9E;
    *(*p)++ = 0xC0;
}

static inline void EmitSetgRax(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x0F;
    *(*p)++ = 0x9F;
    *(*p)++ = 0xC0;
}

static inline void EmitSetgeRax(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x0F;
    *(*p)++ = 0x9D;
    *(*p)++ = 0xC0;
}

static inline void EmitNegRax(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0xF7;
    *(*p)++ = 0xD8;
}

static inline void EmitNotRax(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0xF7;
    *(*p)++ = 0xD0;
}

static inline void EmitMovRaxPtrRax(u8 **p) {
    *(*p)++ = 0x48;
    *(*p)++ = 0x8B;
    *(*p)++ = 0x00;
}

static inline void EmitMovPtrRax(u8 **p, u64 addr) {
    *(*p)++ = 0x48;
    *(*p)++ = 0xA3;
    *(u64*)(*p) = addr;
    *p += 8;
}

static inline void EmitRet(u8 **p) {
    *(*p)++ = 0xC3;
}