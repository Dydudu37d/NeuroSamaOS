#pragma once

#include "int.h"

#define MaxTaskCount 256
#define UesrTaskStart 50
#define SystemTaskStart 0
#define SystemTaskEnd 49

#define ERROR(x) x>MaxTaskCount

typedef struct{
    void (*CallFunc)(void*);
    void (*DelFunc)(void*);
    void *Arg;
    _Bool Active;
    const char* Name;
    u64 NextWaitMs;
    u64 IntervalMs;
} Task;

u16 TaskAdd(Task AddTask,_Bool SystemTask);
void TaskDel(u8 IdxTask);
void TaskPoll();

