#pragma once

#include "int.h"

#define MaxTaskCount 256
#define UserTaskStart 50
#define SystemTaskStart 0
#define SystemTaskEnd 49

#define ERROR(x) x>MaxTaskCount

typedef struct{
    void (*CallFunc)(void*);
    void (*DelFunc)(void*);
    void *Arg;
    _Bool Active;
    const char* Name;
    u64 NextWaitNs;
    u64 IntervalNs;
    
} Task;

u16 TaskAdd(Task AddTask,_Bool SystemTask);
void TaskDel(u8 IdxTask);
void TaskPoll();

