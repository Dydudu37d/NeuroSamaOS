#include "JIT.h"
#include "int.h"
#include "str.h"
#include "debug.h"

__attribute__((aligned(64))) JITVar Vars[1<<20]={0};
u64 VarCount=0;
u64 VarEnd=0;

__attribute__((aligned(64))) JITFunc Funcs[1<<20]={0};
u64 FuncCount=0;
u64 FuncEnd=0;

u64 GetVarCount(){
    return VarCount;
}

void SetVarDataPoint(u64 SerialNumber, u64* DataPoint){
    for (u64 Idx=0; Idx<VarEnd; Idx++){
        if (Vars[Idx].SerialNumber == SerialNumber) {
            Vars[Idx].DataPoint = DataPoint;
        }
    }
}

void* GetVarDataPoint(u64 SerialNumber){
    for (u64 Idx=0; Idx<VarEnd; Idx++){
        if (Vars[Idx].SerialNumber == SerialNumber) {
            return Vars[Idx].DataPoint;
        }
    }
    return 0;
}

u8 GetVarType(u64 SerialNumber){
    for (u64 Idx=0; Idx<VarEnd; Idx++){
        if (Vars[Idx].SerialNumber == SerialNumber) {
            return Vars[Idx].Type;
        }
    }
    return 0;
}

void SetVarType(u64 SerialNumber, u8 Type){
    for (u64 Idx=0; Idx<VarEnd; Idx++){
        if (Vars[Idx].SerialNumber == SerialNumber) {
            Vars[Idx].Type = Type;
        }
    }
}

u64 VarAdd(JITVar Var){
    if (!Var.Type) return (u64)-1;
    for (u64 Idx=0; Idx < (1<<20); Idx++){
        if (!Vars[Idx].Type) {
            Vars[Idx].SerialNumber = Idx;
            Vars[Idx].Type = Var.Type;
            Vars[Idx].DataPoint = Var.DataPoint;
            if (Idx >= VarEnd) VarEnd = Idx + 1;
            VarCount++;
            return Idx;
        }
    }
    return (u64)-1;
}

static inline u64 VarFoundEnd(){
    for (s64 Idx=(1<<20)-1; Idx>=0; Idx--){
        if (Vars[Idx].Type) return Idx + 1;
    }
    return 0;
}

void VarDel(u64 SerialNumber){
    for (u64 Idx=0; Idx<VarEnd; Idx++){
        if (Vars[Idx].SerialNumber == SerialNumber) {
            if (Vars[Idx].Type) VarCount--;
            Vars[Idx].Type = 0;
            Vars[Idx].SerialNumber = 0;
            if (Idx == VarEnd - 1) VarEnd = VarFoundEnd();
            return;
        }
    }
}

u64 GetFuncCount(){
    return FuncCount;
}

void SetFuncPoint(u64 SerialNumber, u64* FuncPoint){
    for (u64 Idx=0; Idx<FuncEnd; Idx++){
        if (Funcs[Idx].SerialNumber == SerialNumber) {
            Funcs[Idx].FuncPoint = FuncPoint;
        }
    }
}

void* GetFuncPoint(u64 SerialNumber){
    for (u64 Idx=0; Idx<FuncEnd; Idx++){
        if (Funcs[Idx].SerialNumber == SerialNumber) {
            return Funcs[Idx].FuncPoint;
        }
    }
    return 0;
}

u8 GetFuncType(u64 SerialNumber){
    for (u64 Idx=0; Idx<FuncEnd; Idx++){
        if (Funcs[Idx].SerialNumber == SerialNumber) {
            return Funcs[Idx].Type;
        }
    }
    return 0;
}

void SetFuncType(u64 SerialNumber, u8 Type){
    for (u64 Idx=0; Idx<FuncEnd; Idx++){
        if (Funcs[Idx].SerialNumber == SerialNumber) {
            Funcs[Idx].Type = Type;
        }
    }
}

u64 FuncAdd(JITFunc Func){
    if (!Func.Type) return (u64)-1;
    for (u64 Idx=0; Idx < (1<<20); Idx++){
        if (!Funcs[Idx].Type) {
            Funcs[Idx].SerialNumber = Idx;
            Funcs[Idx].Type = Func.Type;
            Funcs[Idx].FuncPoint = Func.FuncPoint;
            if (Idx >= FuncEnd) FuncEnd = Idx + 1;
            FuncCount++;
            return Idx;
        }
    }
    return (u64)-1;
}

static inline u64 FuncFoundEnd(){
    for (s64 Idx=(1<<20)-1; Idx>=0; Idx--){
        if (Funcs[Idx].Type) return Idx + 1;
    }
    return 0;
}

void FuncDel(u64 SerialNumber){
    for (u64 Idx=0; Idx<FuncEnd; Idx++){
        if (Funcs[Idx].SerialNumber == SerialNumber) {
            if (Funcs[Idx].Type) FuncCount--;
            Funcs[Idx].Type = 0;
            Funcs[Idx].SerialNumber = 0;
            if (Idx == FuncEnd - 1) FuncEnd = FuncFoundEnd();
            return;
        }
    }
}