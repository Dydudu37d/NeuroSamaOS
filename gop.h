#pragma once

#include "int.h"

#define RGBA2ABGR(rgba) __builtin_bswap32(rgba)

void GOPPixel(u32 x,u32 y,u32 rgba);
void GOPRect(u32 x[2],u32 y[2],u32 rgba);
void GOPRectFill(u32 x[2],u32 y[2],u32 rgba);
void GOPLine(u32 x[2],u32 y[2],u32 rgba);
void GOPClear(u32 rgba);
void GOPFlash();

