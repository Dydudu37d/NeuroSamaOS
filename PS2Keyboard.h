#pragma once

#include "int.h"
#include "PS2.h"

void PS2KeyboardSetLED(u8 Which);
void *PS2KeyboardPoll(void *Arg);
void PS2KeyboardInit();