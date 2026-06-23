#pragma once

#include "int.h"

#define RGBA2ABGR(argb) __builtin_bswap32(argb)

static inline u32 ARGBMix(u32 dst, u32 src) {
    u32 sA = (src >> 24) & 0xFF;
    if (sA == 255) return src;
    if (sA == 0)   return dst;

    u32 dA = 255 - sA;

    u32 srcRGB = src & 0x00FFFFFF;
    u32 dstRGB = dst & 0x00FFFFFF;

    u32 r = (((srcRGB >> 16) & 0xFF) * sA + ((dstRGB >> 16) & 0xFF) * dA + 128) >> 8;
    u32 g = (((srcRGB >> 8)  & 0xFF) * sA + ((dstRGB >> 8)  & 0xFF) * dA + 128) >> 8;
    u32 b = ((srcRGB & 0xFF) * sA + (dstRGB & 0xFF) * dA + 128) >> 8;

    return (sA << 24) | (r << 16) | (g << 8) | b;
}

void GOPPixel(u32 x,u32 y,u32 argb);
void GOPRect(u32 x[2],u32 y[2],u32 argb);
void GOPRectFill(u32 x[2],u32 y[2],u32 argb);
void GOPLine(u32 x[2],u32 y[2],u32 argb);
void GOPClear(u32 argb);
void GOPClearAlpha(u32 argb);
void GOPFlash();

