#include "task.h"
#include "clock.h"

Task Tasks[MaxTaskCount];

u16 TaskAdd(Task AddTask,_Bool SystemTask) {
    for (int i = SystemTaskStart+(UesrTaskStart*(!SystemTask)); i < MaxTaskCount; i++) {
        if (!Tasks[i].Active) {
            Tasks[i] = AddTask;
            Tasks[i].Active = true;
            return i;
        }
    }
    return -1;
}

void TaskDel(u8 IdxTask){
    Tasks[IdxTask].Active=false;
}

void TaskPoll(void) {
    u64 now = SystemGetTimeMillis();
    for (int i = 0; i < MaxTaskCount; i++) {
        if (!Tasks[i].Active) continue;
        if (now >= Tasks[i].NextWaitMs) {
            Tasks[i].CallFunc(Tasks[i].Arg);
            if (Tasks[i].IntervalMs > 0) {
                Tasks[i].NextWaitMs = now + Tasks[i].IntervalMs;
            } else {
                Tasks[i].Active = false;
            }
        }
    }
}
