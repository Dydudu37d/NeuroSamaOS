#include "task.h"
#include "clock.h"
#include "idt.h"
#include "str.h"
#include "debug.h"
#include "Context.h"

extern _Bool IDTGotError;
extern RegContext MainGlobalContext;

Task Tasks[MaxTaskCount]={0};

u16 TaskAdd(Task AddTask, _Bool SystemTask){
    u16 StartIdx = SystemTask ? SystemTaskStart : UserTaskStart;
    u16 EndIdx = SystemTask ? UserTaskStart : MaxTaskCount;
    
    for (u16 i = StartIdx; i < EndIdx; i++){
        if (!Tasks[i].Active) {
            MemCopy(&Tasks[i], &AddTask, sizeof(Task));
            Tasks[i].Active = true;
            return i;
        }
    }
    return (u16)-1;
}

void TaskDel(u8 IdxTask){ 
    Tasks[IdxTask].Active = false;
    if (Tasks[IdxTask].DelFunc) Tasks[IdxTask].DelFunc(Tasks[IdxTask].Arg);
}

void TaskPoll(void){
    u64 Now = SystemGetTimeNano();
    SaveContext(&MainGlobalContext);
    
    for (u16 i = 0; i < MaxTaskCount; i++){
        if (!Tasks[i].Active) continue;
        
        if (Now >= Tasks[i].NextWaitNs){
            if (Tasks[i].CallFunc) {
                if(Tasks[i].Target) Tasks[i].Target=Tasks[i].CallFunc(Tasks[i].Arg);
                else Tasks[i].CallFunc(Tasks[i].Arg);
            }
            
            if (Tasks[i].IntervalNs > 0){
                Tasks[i].NextWaitNs = Now + Tasks[i].IntervalNs;
            }
            else{
                Tasks[i].Active = false;
                if (Tasks[i].DelFunc) Tasks[i].DelFunc(Tasks[i].Arg);
            }
            
            if (IDTGotError){
                Tasks[i].Active = false;
                IDTCloseError();
                LoadContext(MainGlobalContext);
            }
        }
    }
}
