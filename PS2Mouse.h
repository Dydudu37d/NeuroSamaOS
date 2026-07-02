#pragma once

#include "int.h"

typedef struct {
    u8 LButton, RButton, MButton;
    s16 x, y;
} MouseState;

void PS2MouseWrite(u8 data);
u8 PS2MouseRead(void);
void *PS2GetMouseState(void *Arg);
void PS2MouseInit(void);
void PS2MouseSetDPI(u8 DPI);