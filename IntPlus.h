#pragma once

#include "int.h"

typedef struct {
    u64 High;
    u64 Low;
} __attribute__((packed)) u128;

typedef struct {
    s64 High;
    u64 Low;
} __attribute__((packed)) s128;

typedef struct {
    u64 HighHigh;
    u64 HighLow;
    u64 LowHigh;
    u64 LowLow;
} __attribute__((packed)) u256;

typedef struct {
    s64 HighHigh;
    u64 HighLow;
    u64 LowHigh;
    u64 LowLow;
} __attribute__((packed)) s256;

typedef struct {
    u64 U1;
    u64 U2;
} __attribute__((packed)) u64x2;

typedef struct {
    s64 S1;
    s64 S2;
} __attribute__((packed)) s64x2;

typedef struct {
    f64 S1;
    f64 S2;
} __attribute__((packed)) f64x2;

typedef struct {
    u32 U1;
    u32 U2;
    u32 U3;
    u32 U4;
} __attribute__((packed)) u32x4;

typedef struct {
    s32 S1;
    s32 S2;
    s32 S3;
    s32 S4;
} __attribute__((packed)) s32x4;

typedef struct {
    f32 S1;
    f32 S2;
    f32 S3;
    f32 S4;
} __attribute__((packed)) f32x4;

typedef struct {
    u16 U1;
    u16 U2;
    u16 U3;
    u16 U4;
    u16 U5;
    u16 U6;
    u16 U7;
    u16 U8;
} __attribute__((packed)) u16x8;

typedef struct {
    s16 S1;
    s16 S2;
    s16 S3;
    s16 S4;
    s16 S5;
    s16 S6;
    s16 S7;
    s16 S8;
} __attribute__((packed)) s16x8;

typedef struct {
    u8 U1;
    u8 U2;
    u8 U3;
    u8 U4;
    u8 U5;
    u8 U6;
    u8 U7;
    u8 U8;
    u8 U9;
    u8 U10;
    u8 U11;
    u8 U12;
    u8 U13;
    u8 U14;
    u8 U15;
    u8 U16;
} __attribute__((packed)) u8x16;

typedef struct {
    s8 S1;
    s8 S2;
    s8 S3;
    s8 S4;
    s8 S5;
    s8 S6;
    s8 S7;
    s8 S8;
    s8 S9;
    s8 S10;
    s8 S11;
    s8 S12;
    s8 S13;
    s8 S14;
    s8 S15;
    s8 S16;
} __attribute__((packed)) s8x16;

typedef struct {
    u64 U1;
    u64 U2;
    u64 U3;
    u64 U4;
} __attribute__((packed)) u64x4;

typedef struct {
    s64 S1;
    s64 S2;
    s64 S3;
    s64 S4;
} __attribute__((packed)) s64x4;

typedef struct {
    f64 S1;
    f64 S2;
    f64 S3;
    f64 S4;
} __attribute__((packed)) f64x4;

typedef struct {
    u32 U1;
    u32 U2;
    u32 U3;
    u32 U4;
    u32 U5;
    u32 U6;
    u32 U7;
    u32 U8;
} __attribute__((packed)) u32x8;

typedef struct {
    s32 S1;
    s32 S2;
    s32 S3;
    s32 S4;
    s32 S5;
    s32 S6;
    s32 S7;
    s32 S8;
} __attribute__((packed)) s32x8;

typedef struct {
    f32 S1;
    f32 S2;
    f32 S3;
    f32 S4;
    f32 S5;
    f32 S6;
    f32 S7;
    f32 S8;
} __attribute__((packed)) f32x8;

typedef struct {
    u16 U1;
    u16 U2;
    u16 U3;
    u16 U4;
    u16 U5;
    u16 U6;
    u16 U7;
    u16 U8;
    u16 U9;
    u16 U10;
    u16 U11;
    u16 U12;
    u16 U13;
    u16 U14;
    u16 U15;
    u16 U16;
} __attribute__((packed)) u16x16;

typedef struct {
    s16 S1;
    s16 S2;
    s16 S3;
    s16 S4;
    s16 S5;
    s16 S6;
    s16 S7;
    s16 S8;
    s16 S9;
    s16 S10;
    s16 S11;
    s16 S12;
    s16 S13;
    s16 S14;
    s16 S15;
    s16 S16;
} __attribute__((packed)) s16x16;

typedef struct {
    u8 U1;
    u8 U2;
    u8 U3;
    u8 U4;
    u8 U5;
    u8 U6;
    u8 U7;
    u8 U8;
    u8 U9;
    u8 U10;
    u8 U11;
    u8 U12;
    u8 U13;
    u8 U14;
    u8 U15;
    u8 U16;
    u8 U17;
    u8 U18;
    u8 U19;
    u8 U20;
    u8 U21;
    u8 U22;
    u8 U23;
    u8 U24;
    u8 U25;
    u8 U26;
    u8 U27;
    u8 U28;
    u8 U29;
    u8 U30;
    u8 U31;
    u8 U32;
} __attribute__((packed)) u8x32;

typedef struct {
    s8 S1;
    s8 S2;
    s8 S3;
    s8 S4;
    s8 S5;
    s8 S6;
    s8 S7;
    s8 S8;
    s8 S9;
    s8 S10;
    s8 S11;
    s8 S12;
    s8 S13;
    s8 S14;
    s8 S15;
    s8 S16;
    s8 S17;
    s8 S18;
    s8 S19;
    s8 S20;
    s8 S21;
    s8 S22;
    s8 S23;
    s8 S24;
    s8 S25;
    s8 S26;
    s8 S27;
    s8 S28;
    s8 S29;
    s8 S30;
    s8 S31;
    s8 S32;
} __attribute__((packed)) s8x32;

typedef signed __int128 Cs128;
typedef unsigned __int128 Cu128;

typedef u64 uv256 __attribute__((vector_size(32)));
typedef u64 uv128 __attribute__((vector_size(16)));

typedef u64 uv64x4 __attribute__((vector_size(32)));
typedef u64 uv64x2 __attribute__((vector_size(16)));
typedef f64 fv64x4 __attribute__((vector_size(32)));
typedef f64 fv64x2 __attribute__((vector_size(16)));

typedef u32 uv32x8 __attribute__((vector_size(32)));
typedef u32 uv32x4 __attribute__((vector_size(16)));
typedef f32 fv32x8 __attribute__((vector_size(32)));
typedef f32 fv32x4 __attribute__((vector_size(16)));

typedef u16 uv16x16 __attribute__((vector_size(32)));
typedef u16 uv16x8 __attribute__((vector_size(16)));

typedef u8 uv8x32 __attribute__((vector_size(32)));
typedef u8 uv8x16 __attribute__((vector_size(16)));
