#pragma once

#include "int.h"

#define ARGB2BGRA(argb) __builtin_bswap32(argb)

#define HDR_R_SHIFT  48
#define HDR_G_SHIFT  32
#define HDR_B_SHIFT  16
#define HDR_A_SHIFT  0

#define HDR_R_MASK  0xFFFF000000000000ULL
#define HDR_G_MASK  0x0000FFFF00000000ULL
#define HDR_B_MASK  0x00000000FFFF0000ULL
#define HDR_A_MASK  0x000000000000FFFFULL

typedef u64 HDR_PIXEL;

static inline HDR_PIXEL HDR_Pack(u16 r, u16 g, u16 b, u16 a) {
    return ((u64)r << HDR_R_SHIFT) |
           ((u64)g << HDR_G_SHIFT) |
           ((u64)b << HDR_B_SHIFT) |
           ((u64)a << HDR_A_SHIFT);
}

static inline u16 HDR_Unpack_R(HDR_PIXEL p) {
    return (p >> HDR_R_SHIFT) & 0xFFFF;
}

static inline u16 HDR_Unpack_G(HDR_PIXEL p) {
    return (p >> HDR_G_SHIFT) & 0xFFFF;
}

static inline u16 HDR_Unpack_B(HDR_PIXEL p) {
    return (p >> HDR_B_SHIFT) & 0xFFFF;
}

static inline u16 HDR_Unpack_A(HDR_PIXEL p) {
    return (p >> HDR_A_SHIFT) & 0xFFFF;
}

static inline HDR_PIXEL ARGB_To_HDR(u32 argb) {
    u16 r = (((argb >> 16) & 0xFF) * 65535) / 255;
    u16 g = (((argb >> 8) & 0xFF) * 65535) / 255;
    u16 b = ((argb & 0xFF) * 65535) / 255;
    u16 a = (((argb >> 24) & 0xFF) * 65535) / 255;
    return HDR_Pack(r, g, b, a);
}

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

static inline u8 GetBitCountFromMask(u32 mask) {
    return __builtin_popcount(mask);
}

void GOPPixel(u32 x, u32 y, HDR_PIXEL hdr);
void GOPRect(u32 x[2], u32 y[2], HDR_PIXEL hdr);
void GOPRectFill(u32 x[2], u32 y[2], HDR_PIXEL hdr);
void GOPLine(u32 x[2], u32 y[2], HDR_PIXEL hdr);
void GOPClear(HDR_PIXEL hdr);
void GOPClearAlpha(HDR_PIXEL hdr);
void GOPClearAlpha(HDR_PIXEL hdr);
void GOPFlash();