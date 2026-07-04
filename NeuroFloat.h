#pragma once

#include "int.h"

#define FP8_MSB_MOVE (7)
#define FP8_POINT_MOVE (4)
#define FP8_MANTISSA_MOVE (0)

#define FP8_MSB(x) ((x&0b1)<<FP8_MSB_MOVE)
#define FP8_POINT(x) ((x&0b111)<<FP8_POINT_MOVE)
#define FP8_MANTISSA(x) ((x&0b1111)<<FP8_MANTISSA_MOVE)

#define FP16_MSB_MOVE (15)
#define FP16_POINT_MOVE (11)
#define FP16_MANTISSA_MOVE (0)

#define FP16_MSB(x) ((x&0b1)<<FP16_MSB_MOVE)
#define FP16_POINT(x) ((x&0b1111)<<FP16_POINT_MOVE)
#define FP16_MANTISSA(x) ((x&0b11111111111)<<FP16_MANTISSA_MOVE)

#define FP32_MSB_MOVE      (31)
#define FP32_POINT_MOVE    (26)
#define FP32_MANTISSA_MOVE (0)

#define FP32_MSB(x)        (((u32)(x) & 0b1) << FP32_MSB_MOVE)
#define FP32_POINT(x)      (((u32)(x) & 0b11111) << FP32_POINT_MOVE)
#define FP32_MANTISSA(x)   (((u32)(x) & 0b11111111111111111111111111) << FP32_MANTISSA_MOVE)

#define FP64_MSB_MOVE      (63)
#define FP64_POINT_MOVE    (57)
#define FP64_MANTISSA_MOVE (0)

#define FP64_MSB(x)        (((u64)(x) & 0b1) << FP64_MSB_MOVE)
#define FP64_POINT(x)      (((u64)(x) & 0b111111) << FP64_POINT_MOVE)
#define FP64_MANTISSA(x)   (((u64)(x) & 0b111111111111111111111111111111111111111111111111111111111) << FP64_MANTISSA_MOVE)

typedef struct {
    union {
        u8 U;
        struct {
            u8 Mantissa : 4;
            u8 Point    : 3;
            u8 MSB      : 1;
        };
    };
} FP8;

typedef struct {
    union {
        u16 U;
        struct {
            u16 Mantissa : 11;
            u16 Point    : 4;
            u16 MSB      : 1;
        };
    };
} FP16;

typedef struct {
    union {
        u32 U;
        struct {
            u32 Mantissa : 26;
            u32 Point    : 5;
            u32 MSB      : 1;
        };
    };
} FP32;

typedef struct {
    union {
        u64 U;
        struct {
            u64 Mantissa : 57;
            u64 Point    : 6;
            u64 MSB      : 1;
        };
    };
} FP64;
