#pragma once

#include "int.h"
#include "gui.h"

typedef struct {
    u64 WindowEnd;
    u64 Count;
    Window **Windows;
} WM;

void WMAddWindow(WM* WinM,Window* Win);
void WMDelWindow(WM* WinM,const char* Title);
void WMPoll(WM* WinM);
