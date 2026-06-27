#include "wm.h"
#include "int.h"
#include "gui.h"
#include "str.h"

void WMAddWindow(WM *WinM, Window *Win){
    for (u64 Idx=0;Idx<WinM->WindowEnd+1;Idx++){
        if (!WinM->Windows[Idx]) {
            WinM->Windows[Idx]=Win;
            WinM->Count++;
            if (Idx>=WinM->WindowEnd) WinM->WindowEnd=Idx;
            return;
        }
    }
}

void WMDelWindow(WM *WinM, const char *Title){
    for (u64 Idx=0;Idx<WinM->WindowEnd;Idx++){
        if (!WinM->Windows[Idx]) continue;
        if (StrIs(WinM->Windows[Idx]->Title,Title)) WinM->Windows[Idx]=NULL;
        WinM->Count--;
        return;
    }
}

void WMPoll(WM *WinM){
    for (u64 Idx=0;Idx<WinM->WindowEnd;Idx++){
        if ((!WinM->Windows[Idx]) || (!WinM->Windows[Idx]->Poll) || (!WinM->Windows[Idx]->Base.Draw)) continue;
        WinM->Windows[Idx]->Poll(WinM->Windows[Idx]);
        WinM->Windows[Idx]->Base.Draw(WinM->Windows[Idx]);
    }
}
