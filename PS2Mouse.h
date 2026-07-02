#pragma once

#include "int.h"

typedef struct {
    u8 LButton, RButton, MButton;
    s16 x, y;
} MouseState;

void PS2MouseWrite(u8 data);
u8 PS2MouseRead();
MouseState PS2GetMouseState();
void PS2MouseInit();
void PS2MouseSetDPI(u8 DPI);