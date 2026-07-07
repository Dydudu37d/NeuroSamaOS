#pragma once

#include "int.h"

typedef struct {
    union {
        u8 U;
        struct {
            u8 Mantissa : 4;
            u8 Point    : 3;
            u8 MSB      : 1;
        }__attribute__((packed));
    };
} FP8;

typedef struct {
    union {
        u16 U;
        struct {
            u16 Mantissa : 11;
            u16 Point    : 4;
            u16 MSB      : 1;
        }__attribute__((packed));
    };
} FP16;

typedef struct {
    union {
        u32 U;
        struct {
            u32 Mantissa : 26;
            u32 Point    : 5;
            u32 MSB      : 1;
        }__attribute__((packed));
    };
} FP32;

typedef struct {
    union {
        u64 U;
        struct {
            u64 Mantissa : 57;
            u64 Point    : 6;
            u64 MSB      : 1;
        }__attribute__((packed));
    };
} FP64;

static inline _Bool FP8PrecisionIs(FP8 F1, FP8 F2){
    return F1.Point == F2.Point;
}

static inline _Bool FP16PrecisionIs(FP16 F1, FP16 F2){
    return F1.Point == F2.Point;
}

static inline _Bool FP32PrecisionIs(FP32 F1, FP32 F2){
    return F1.Point == F2.Point;
}

static inline _Bool FP64PrecisionIs(FP64 F1, FP64 F2){
    return F1.Point == F2.Point;
}


static inline _Bool FP8Is(FP8 F1, FP8 F2){
    if (F1.MSB != F2.MSB) {
        return (F1.Mantissa == 0 && F2.Mantissa == 0);
    }
    if (F1.Point == F2.Point) {
        return F1.Mantissa == F2.Mantissa;
    }
    if (F1.Point < F2.Point) {
        u64 diff = (u64)F2.Point - F1.Point;
        return ((u64)F1.Mantissa << diff) == (u64)F2.Mantissa;
    } else {
        u64 diff = (u64)F1.Point - F2.Point;
        return (u64)F1.Mantissa == ((u64)F2.Mantissa << diff);
    }
}

static inline _Bool FP16Is(FP16 F1, FP16 F2){
    if (F1.MSB != F2.MSB) {
        return (F1.Mantissa == 0 && F2.Mantissa == 0);
    }
    if (F1.Point == F2.Point) {
        return F1.Mantissa == F2.Mantissa;
    }
    if (F1.Point < F2.Point) {
        u64 diff = (u64)F2.Point - F1.Point;
        return ((u64)F1.Mantissa << diff) == (u64)F2.Mantissa;
    } else {
        u64 diff = (u64)F1.Point - F2.Point;
        return (u64)F1.Mantissa == ((u64)F2.Mantissa << diff);
    }
}

static inline _Bool FP32Is(FP32 F1, FP32 F2){
    if (F1.MSB != F2.MSB) {
        return (F1.Mantissa == 0 && F2.Mantissa == 0);
    }
    if (F1.Point == F2.Point) {
        return F1.Mantissa == F2.Mantissa;
    }
    if (F1.Point < F2.Point) {
        u64 diff = (u64)F2.Point - F1.Point;
        return ((u64)F1.Mantissa << diff) == (u64)F2.Mantissa;
    } else {
        u64 diff = (u64)F1.Point - F2.Point;
        return (u64)F1.Mantissa == ((u64)F2.Mantissa << diff);
    }
}


static inline _Bool FP64Is(FP64 F1, FP64 F2){
    if (F1.MSB != F2.MSB) {
        return (F1.Mantissa == 0 && F2.Mantissa == 0);
    }
    if (F1.Point == F2.Point) {
        return F1.Mantissa == F2.Mantissa;
    }
    if (F1.Point < F2.Point) {
        u64 diff = F2.Point - F1.Point;
        if (diff >= 64) return 0; 
        return (F1.Mantissa << diff) == F2.Mantissa;
    } else {
        u64 diff = F1.Point - F2.Point;
        if (diff >= 64) return 0;
        return F1.Mantissa == (F2.Mantissa << diff);
    }
}

static inline FP8 FP8Add(FP8 F1, FP8 F2){
    FP8 result;
    s32 v1 = F1.MSB ? -(s32)F1.Mantissa : (s32)F1.Mantissa;
    s32 v2 = F2.MSB ? -(s32)F2.Mantissa : (s32)F2.Mantissa;
    s32 sum = v1 + v2;
    if (sum < 0) {
        result.MSB = 1;
        result.Mantissa = (u8)(-sum);
    } else {
        result.MSB = 0;
        result.Mantissa = (u8)sum;
    }
    result.Point = F1.Point;
    return result;
}

static inline FP16 FP16Add(FP16 F1, FP16 F2){
    FP16 result;
    s32 v1 = F1.MSB ? -(s32)F1.Mantissa : (s32)F1.Mantissa;
    s32 v2 = F2.MSB ? -(s32)F2.Mantissa : (s32)F2.Mantissa;
    s32 sum = v1 + v2;
    if (sum < 0) {
        result.MSB = 1;
        result.Mantissa = (u16)(-sum);
    } else {
        result.MSB = 0;
        result.Mantissa = (u16)sum;
    }
    result.Point = F1.Point;
    return result;
}

static inline FP32 FP32Add(FP32 F1, FP32 F2){
    FP32 result;
    s64 v1 = F1.MSB ? -(s64)F1.Mantissa : (s64)F1.Mantissa;
    s64 v2 = F2.MSB ? -(s64)F2.Mantissa : (s64)F2.Mantissa;
    s64 sum = v1 + v2;
    if (sum < 0) {
        result.MSB = 1;
        result.Mantissa = (u32)(-sum);
    } else {
        result.MSB = 0;
        result.Mantissa = (u32)sum;
    }
    result.Point = F1.Point;
    return result;
}

static inline FP64 FP64Add(FP64 F1, FP64 F2){
    FP64 result;
    s64 v1 = F1.MSB ? -(s64)F1.Mantissa : (s64)F1.Mantissa;
    s64 v2 = F2.MSB ? -(s64)F2.Mantissa : (s64)F2.Mantissa;
    s64 sum = v1 + v2;
    if (sum < 0) {
        result.MSB = 1;
        result.Mantissa = (u64)(-sum);
    } else {
        result.MSB = 0;
        result.Mantissa = (u64)sum;
    }
    result.Point = F1.Point;
    return result;
}

static inline void FP8Split(FP8 F, u8* integer, u8* fraction) {
    if (integer) {
        *integer = F.Mantissa >> F.Point;
    }
    if (fraction) {
        *fraction = F.Mantissa & ((1 << F.Point) - 1);
    }
}

static inline void FP16Split(FP16 F, u16* integer, u16* fraction) {
    if (integer) {
        *integer = F.Mantissa >> F.Point;
    }
    if (fraction) {
        *fraction = F.Mantissa & ((1 << F.Point) - 1);
    }
}

static inline void FP32Split(FP32 F, u32* integer, u32* fraction) {
    if (integer) {
        *integer = F.Mantissa >> F.Point;
    }
    if (fraction) {
        *fraction = F.Mantissa & ((1 << F.Point) - 1);
    }
}

static inline void FP64Split(FP64 F, u64* integer, u64* fraction) {
    if (F.Point >= 64) {
        if (integer) {
            *integer = 0;
        }
        if (fraction) {
            *fraction = F.Mantissa;
        }
    } else {
        if (integer) {
            *integer = F.Mantissa >> F.Point;
        }
        if (fraction) {
            *fraction = F.Mantissa & ((1ULL << F.Point) - 1);
        }
    }
}

static inline FP8 FP8Sub(FP8 F1, FP8 F2){
    FP8 result;
    s32 v1 = F1.MSB ? -(s32)F1.Mantissa : (s32)F1.Mantissa;
    s32 v2 = F2.MSB ? -(s32)(~F2.Mantissa) : (s32)(~F2.Mantissa);
    s32 sum = v1 + v2;
    if (sum < 0) {
        result.MSB = 1;
        result.Mantissa = (u8)(-sum);
    } else {
        result.MSB = 0;
        result.Mantissa = (u8)sum;
    }
    result.Point = F1.Point;
    return result;
}

static inline FP16 FP16Sub(FP16 F1, FP16 F2){
    FP16 result;
    s32 v1 = F1.MSB ? -(s32)F1.Mantissa : (s32)F1.Mantissa;
    s32 v2 = F2.MSB ? -(s32)(~F2.Mantissa) : (s32)(~F2.Mantissa);
    s32 sum = v1 + v2;
    if (sum < 0) {
        result.MSB = 1;
        result.Mantissa = (u16)(-sum);
    } else {
        result.MSB = 0;
        result.Mantissa = (u16)sum;
    }
    result.Point = F1.Point;
    return result;
}

static inline FP32 FP32Sub(FP32 F1, FP32 F2){
    FP32 result;
    s64 v1 = F1.MSB ? -(s64)F1.Mantissa : (s64)F1.Mantissa;
    s64 v2 = F2.MSB ? -(s64)(~F2.Mantissa) : (s64)(~F2.Mantissa);
    s64 sum = v1 + v2;
    if (sum < 0) {
        result.MSB = 1;
        result.Mantissa = (u32)(-sum);
    } else {
        result.MSB = 0;
        result.Mantissa = (u32)sum;
    }
    result.Point = F1.Point;
    return result;
}

static inline FP64 FP64Sub(FP64 F1, FP64 F2){
    FP64 result;
    s64 v1 = F1.MSB ? -(s64)F1.Mantissa : (s64)F1.Mantissa;
    s64 v2 = F2.MSB ? -(s64)(~F2.Mantissa) : (s64)(~F2.Mantissa);
    s64 sum = v1 + v2;
    if (sum < 0) {
        result.MSB = 1;
        result.Mantissa = (u64)(-sum);
    } else {
        result.MSB = 0;
        result.Mantissa = (u64)sum;
    }
    result.Point = F1.Point;
    return result;
}

static inline _Bool FP8IsExact(FP8 F1, FP8 F2){
    return F1.U == F2.U;
}

static inline _Bool FP16IsExact(FP16 F1, FP16 F2){
    return F1.U == F2.U;
}

static inline _Bool FP32IsExact(FP32 F1, FP32 F2){
    return F1.U == F2.U;
}

static inline _Bool FP64IsExact(FP64 F1, FP64 F2){
    return F1.U == F2.U;
}