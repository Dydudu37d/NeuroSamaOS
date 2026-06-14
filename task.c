#include "task.h"
#include "clock.h"
#include "Context.h"
#include "idt.h"
#include "str.h"

extern RegContext MainGlobalContext;
extern _Bool IDTGotError;

Task Tasks[MaxTaskCount];

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
    
    for (u16 i = 0; i < MaxTaskCount; i++){
        if (!Tasks[i].Active) continue;
        
        if (Now >= Tasks[i].NextWaitNs){
            SaveContext(&MainGlobalContext);
            
            if (Tasks[i].CallFunc) Tasks[i].CallFunc(Tasks[i].Arg);
            
            if (Tasks[i].IntervalNs > 0){
                Tasks[i].NextWaitNs = Now + Tasks[i].IntervalNs;
            }
            else{
                Tasks[i].Active = false;
                if (Tasks[i].DelFunc) Tasks[i].DelFunc(Tasks[i].Arg);
            }
            
            if (IDTGotError){
                LoadContext(MainGlobalContext);
                IDTCloseError();
            }
        }
    }
}
