#include "JIT.h"
#include "int.h"
#include "str.h"

__attribute__((aligned(64))) static JITVar Vars[1<<20]={0};
u64 VarCount=0;
u64 VarEnd=0;

__attribute__((aligned(64))) static JITFunc Funcs[1<<20]={0};
u64 FuncCount=0;
u64 FuncEnd=0;

u64 GetVarCount(){
    return VarCount;
}
void SetVarDataPoint(u64 SerialNumber,u64* DataPoint){
    for (u64 Idx=0;Idx<VarEnd;Idx++){
        if (Vars[Idx].SerialNumber==SerialNumber) Vars[Idx].DataPoint=DataPoint;
    }
}
void* GetVarDataPoint(u64 SerialNumber){
    for (u64 Idx=0;Idx<VarEnd;Idx++){
        if (Vars[Idx].SerialNumber==SerialNumber) return Vars[Idx].DataPoint;
    }
    return 0;
}
u8 GetVarType(u64 SerialNumber){
    for (u64 Idx=0;Idx<VarEnd;Idx++){
        if (Vars[Idx].SerialNumber==SerialNumber) return Vars[Idx].Type;
    }
    return 0;
}
void SetVarType(u64 SerialNumber,u8 Type){
    for (u64 Idx=0;Idx<VarEnd;Idx++){
        if (Vars[Idx].SerialNumber!=SerialNumber) continue;
        Vars[Idx].Type=Type;
    }
}
u64 VarAdd(JITVar Var){
    if (!Var.Type) return -1;
    for (u64 Idx=0;Idx<VarEnd+1;Idx++){
        if (!Vars[Idx].Type) {
            MemCopy(&Vars[Idx], &Var, sizeof(JITVar));
            if (Idx>=VarEnd)VarEnd=Idx;
            VarCount++;
            return Idx;
        }
    }
    return -1;
}
static inline u64 VarFoundEnd(){
    for (s64 Idx=(1<<20)-1;Idx>=0;Idx--) if (Vars[Idx].Type) return Idx;
    return -1;
}
void VarDel(u64 SerialNumber){
    for (u64 Idx=0;Idx<VarEnd+1;Idx++){
        if (Vars[Idx].SerialNumber!=SerialNumber) continue;
        if (Vars[Idx].Type) VarCount--;
        Vars[Idx].Type=0;
        if (Idx==VarEnd) VarEnd=VarFoundEnd();
    }
}

u64 GetFuncCount(){
    return FuncCount;
}
void SetFuncPoint(u64 SerialNumber,u64* FuncPoint){
    for (u64 Idx=0;Idx<FuncEnd;Idx++){
        if (Funcs[Idx].SerialNumber==SerialNumber) Funcs[Idx].FuncPoint=FuncPoint;
    }
}
void* GetFuncPoint(u64 SerialNumber){
    for (u64 Idx=0;Idx<FuncEnd;Idx++){
        if (Funcs[Idx].SerialNumber==SerialNumber) return Funcs[Idx].FuncPoint;
    }
    return NULL;
}
u8 GetFuncType(u64 SerialNumber){
    for (u64 Idx=0;Idx<FuncEnd;Idx++){
        if (Funcs[Idx].SerialNumber==SerialNumber) return Funcs[Idx].Type;
    }
    return 0;
}
void SetFuncType(u64 SerialNumber,u8 Type){
    for (u64 Idx=0;Idx<FuncEnd;Idx++){
        if (Funcs[Idx].SerialNumber!=SerialNumber) continue;
        Funcs[Idx].Type=Type;
    }
}
u64 FuncAdd(JITFunc Func){
    if (!Func.Type) return -1;
    for (u64 Idx=0;Idx<FuncEnd+1;Idx++){
        if (!Funcs[Idx].Type) {
            MemCopy(&Funcs[Idx], &Func, sizeof(JITFunc));
            if (Idx>=FuncEnd) FuncEnd=Idx;
            FuncCount++;
            return Idx;
        }
    }
    return -1;
}
static inline u64 FuncFoundEnd(){
    for (s64 Idx=(1<<20)-1;Idx>=0;Idx--) if (Funcs[Idx].Type) return Idx;
    return -1;
}
void FuncDel(u64 SerialNumber){
    for (u64 Idx=0;Idx<FuncEnd+1;Idx++){
        if (Funcs[Idx].SerialNumber!=SerialNumber) continue;
        if (Funcs[Idx].Type) FuncCount--;
        Funcs[Idx].Type=0;
        if (Idx==FuncEnd) FuncEnd=FuncFoundEnd();
    }
}
