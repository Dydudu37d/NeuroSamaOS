#pragma once

#include "int.h"

#define M_PI 3.1415926535897932384626433832795028841971693993751L
#define M_2PI 6.2831853071795864769252867665590057683943387987502L
#define M_HALF_PI 1.5707963267948966192313216916397514420985846996876L
#define M_PI_4 0.78539816339744830961566084581987572104929234984378L
#define INV_2PI 0.15915494309189533576888376337251436203445964574046L
#define INV_PI 0.31830988618379067153776752674502872406891929148091L
#define LOG2E 1.44269504088896340735992468100189213742664595415299L
#define LN2 0.69314718055994530941723212145817656807550013436026L

static const f64 DOUBLE_ONE = 1.0;
static const f32 FLOAT_ONE = 1.0f;
const f64 inv_2pi = INV_2PI;
const f64 two_pi = M_2PI;
const f64 log_2e = LOG2E;
const f64 inv_pi = INV_PI;

static const f64 CONST_1_OVER_6 = 1.0/6.0;
static const f64 CONST_1_OVER_120 = 1.0/120.0;
static const f64 CONST_1_OVER_5040 = 1.0/5040.0;
static const f64 CONST_1_OVER_362880 = 1.0/362880.0;
static const f64 CONST_1_OVER_40320 = 1.0/40320.0;
static const f64 CONST_1_OVER_720 = 1.0/720.0;
static const f64 CONST_1_OVER_24 = 1.0/24.0;
static const f64 CONST_1_OVER_3628800 = 1.0/3628800.0;
static const f64 CONST_1_OVER_39916800 = 1.0/39916800.0;
static const f64 CONST_1_OVER_479001600 = 1.0/479001600.0;
static const f64 CONST_1_OVER_87178291200 = 1.0/87178291200.0;
static const f64 CONST_1_OVER_3 = 1.0/3.0;
static const f64 CONST_1_OVER_5 = 1.0/5.0;
static const f64 CONST_1_OVER_7 = 1.0/7.0;
static const f64 CONST_1_OVER_9 = 1.0/9.0;
static const f64 CONST_1_OVER_11 = 1.0/11.0;
static const f64 CONST_1_OVER_13 = 1.0/13.0;
static const f64 CONST_0_5 = 0.5;
static const f32 CONST_F32_1_OVER_6 = 1.0f/6.0f;
static const f32 CONST_F32_1_OVER_120 = 1.0f/120.0f;
static const f32 CONST_F32_1_OVER_5040 = 1.0f/5040.0f;
static const f32 CONST_F32_1_OVER_362880 = 1.0f/362880.0f;
static const f32 CONST_F32_NEG_1 = -1.0f;
static const f64 CONST_1 = 1.0;

static inline s64 S64Abs(s64 x) {
    s64 result;
    __asm__ volatile (
        "movq       %1,       %%xmm0\n\t"
        "pabsq      %%xmm0,   %%xmm0\n\t"
        "movq       %%xmm0,   %0\n\t"
        : "=r"(result)
        : "r"(x)
        : "xmm0"
    );
    return result;
}

static inline u64 U64Pow(u64 x, u64 exp) {
    u64 result = 1;
    while (exp) {
        if (exp & 1) result *= x;
        x *= x;
        exp >>= 1;
    }
    return result;
}

static inline f64 DoublePow(f64 x, u64 time) {
    f64 result = 1.0;
    f64 base = x;
    while (time > 0) {
        if (time & 1) result *= base;
        base *= base;
        time >>= 1;
    }
    return result;
}

static inline f64 DoubleAbs(f64 x) {
    const unsigned long long mask = 0x7FFFFFFFFFFFFFFFULL;
    f64 result;
    __asm__ volatile (
        "andpd   %1, %0"
        : "=x"(result)
        : "0"(x), "m"(mask)
    );
    return result;
}

static inline f64 DoubleMod(f64 x, f64 y) {
    if (!y || !x) return 0.0;
    
    f64 result;
    __asm__ volatile (
        "movsd %1, %%xmm0\n\t"
        "movsd %2, %%xmm1\n\t"
        "divsd %%xmm1, %%xmm0\n\t"
        "roundsd $0x08, %%xmm0, %%xmm0\n\t"
        "mulsd %%xmm1, %%xmm0\n\t"
        "movsd %1, %%xmm1\n\t"
        "subsd %%xmm0, %%xmm1\n\t"
        "movsd %%xmm1, %0"
        : "=m"(result)
        : "m"(x), "m"(y)
        : "xmm0", "xmm1", "memory"
    );
    return result;
}

static inline f64 DoubleMod2PI(f64 x) {
    f64 result;
    const f64 inv_2pi = INV_2PI;
    const f64 two_pi = M_2PI;
    
    __asm__ volatile (
        "movsd %1, %%xmm0\n\t"
        "movsd %2, %%xmm1\n\t"
        "mulsd %%xmm1, %%xmm0\n\t"
        "roundsd $0x08, %%xmm0, %%xmm0\n\t"
        "movsd %3, %%xmm1\n\t"
        "mulsd %%xmm1, %%xmm0\n\t"
        "movsd %1, %%xmm1\n\t"
        "subsd %%xmm0, %%xmm1\n\t"
        "movsd %%xmm1, %0"
        : "=m"(result)
        : "x"(x), "m"(inv_2pi), "m"(two_pi)
        : "xmm0", "xmm1", "memory"
    );
    return result;
}

static inline f64 DoubleSin(f64 x) {
    if (x != x) return x;
    if (x > 1e7f || x < -1e7f) return 0.0f;
    
    f64 result;
    const f64 inv_2pi = INV_2PI;
    const f64 two_pi = M_2PI;
    const f32 neg_one = -1.0f;
    const f32 one = 1.0f;
    const f32 pi = 3.141592653589793f;
    const f32 half_pi = 1.570796326794896f;
    const f32 c1 = 1.0f/362880.0f;
    const f32 c2 = 1.0f/5040.0f;
    const f32 c3 = 1.0f/120.0f;
    const f32 c4 = 1.0f/6.0f;
    
    __asm__ volatile (
        "movss       %1,       %%xmm0\n\t"
        "movss       %2,       %%xmm1\n\t"
        "mulss       %%xmm1,   %%xmm0\n\t"
        "cvttss2si   %%xmm0,   %%eax\n\t"
        "cvtsi2ss    %%eax,    %%xmm1\n\t"
        "movss       %3,       %%xmm2\n\t"
        "mulss       %%xmm1,   %%xmm2\n\t"
        "movss       %1,       %%xmm0\n\t"
        "subss       %%xmm2,   %%xmm0\n\t"
        
        "xorps       %%xmm2,   %%xmm2\n\t"
        "ucomiss     %%xmm2,   %%xmm0\n\t"
        "jae         1f\n\t"
        "mulss       %4,       %%xmm0\n\t"
        "movss       %4,       %%xmm2\n\t"
        "jmp         2f\n\t"
        "1:\n\t"
        "movss       %5,       %%xmm2\n\t"
        "2:\n\t"
        
        "movss       %6,       %%xmm3\n\t"
        "ucomiss     %%xmm3,   %%xmm0\n\t"
        "jbe         3f\n\t"
        "movss       %3,       %%xmm1\n\t"
        "subss       %%xmm0,   %%xmm1\n\t"
        "movss       %%xmm1,   %%xmm0\n\t"
        "mulss       %4,       %%xmm2\n\t"
        "3:\n\t"
        
        "movss       %7,       %%xmm3\n\t"
        "ucomiss     %%xmm3,   %%xmm0\n\t"
        "jbe         4f\n\t"
        "movss       %6,       %%xmm1\n\t"
        "subss       %%xmm0,   %%xmm1\n\t"
        "movss       %%xmm1,   %%xmm0\n\t"
        "4:\n\t"
        
        "movss       %%xmm0,   %%xmm1\n\t"
        "mulss       %%xmm1,   %%xmm1\n\t"
        
        "movss       %8,       %%xmm3\n\t"
        "mulss       %%xmm1,   %%xmm3\n\t"
        "movss       %9,       %%xmm4\n\t"
        "subss       %%xmm3,   %%xmm4\n\t"
        "mulss       %%xmm1,   %%xmm4\n\t"
        "movss       %10,      %%xmm3\n\t"
        "addss       %%xmm4,   %%xmm3\n\t"
        "mulss       %%xmm1,   %%xmm3\n\t"
        "movss       %11,      %%xmm4\n\t"
        "subss       %%xmm3,   %%xmm4\n\t"
        "mulss       %%xmm1,   %%xmm4\n\t"
        "movss       %5,       %%xmm3\n\t"
        "subss       %%xmm4,   %%xmm3\n\t"
        "mulss       %%xmm0,   %%xmm3\n\t"
        "mulss       %%xmm2,   %%xmm3\n\t"
        
        "movss       %%xmm3,   %0"
        : "=m"(result)
        : "m"(x),
          "m"(inv_2pi),
          "m"(two_pi),
          "m"(neg_one),
          "m"(one),
          "m"(pi),
          "m"(half_pi),
          "m"(c1),
          "m"(c2),
          "m"(c3),
          "m"(c4)
        : "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "memory"
    );
    
    return result;
}

static inline f64 DoubleCos(f64 x) {
    x = DoubleMod2PI(x);
    
    f64 sign = 1.0;
    if (x < 0) x = -x;
    if (x > M_PI) {
        x = M_2PI - x;
        sign = -sign;
    }
    if (x > M_HALF_PI) {
        x = M_PI - x;
    }
    
    f64 x2 = x * x;
    f64 result;
    
    __asm__ volatile (
        "movsd %1, %%xmm1\n\t"
        "movsd %2, %%xmm2\n\t"
        "movsd %3, %%xmm3\n\t"
        "movsd %4, %%xmm4\n\t"
        "movsd %5, %%xmm5\n\t"
        "movsd %6, %%xmm6\n\t"
        "movsd %7, %%xmm7\n\t"
        
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %3, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %4, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %5, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %6, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %7, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "movsd %8, %%xmm3\n\t"
        "addsd %%xmm3, %%xmm2\n\t"
        "movsd %%xmm2, %0"
        : "=m"(result)
        : "m"(x2),
          "m"(CONST_1_OVER_87178291200),
          "m"(CONST_1_OVER_479001600),
          "m"(CONST_1_OVER_39916800),
          "m"(CONST_1_OVER_3628800),
          "m"(CONST_1_OVER_40320),
          "m"(CONST_1_OVER_720),
          "m"(CONST_1_OVER_24),
          "m"(CONST_0_5),
          "m"(DOUBLE_ONE)
        : "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory"
    );
    
    return sign * result;
}

static inline f32 FloatSin(f32 x) {
    f32 result;
    const f32 inv_pi = INV_PI;
    const f32 one = 1.0f;
    const f32 c1 = 1.0f/5040.0f;
    const f32 c2 = 1.0f/120.0f;
    const f32 c3 = 1.0f/6.0f;
    
    __asm__ volatile (
        "movss %1, %%xmm0\n\t"
        "movss %2, %%xmm1\n\t"
        "mulss %%xmm1, %%xmm0\n\t"
        "roundss $0x09, %%xmm0, %%xmm0\n\t"
        "movss %3, %%xmm1\n\t"
        "subss %%xmm1, %%xmm0\n\t"
        "movss %2, %%xmm1\n\t"
        "mulss %%xmm1, %%xmm0\n\t"
        "movss %%xmm0, %%xmm1\n\t"
        "mulss %%xmm1, %%xmm1\n\t"
        "movss %4, %%xmm2\n\t"
        "mulss %%xmm1, %%xmm2\n\t"
        "movss %5, %%xmm3\n\t"
        "subss %%xmm2, %%xmm3\n\t"
        "mulss %%xmm1, %%xmm3\n\t"
        "movss %6, %%xmm2\n\t"
        "addss %%xmm3, %%xmm2\n\t"
        "mulss %%xmm1, %%xmm2\n\t"
        "movss %7, %%xmm3\n\t"
        "subss %%xmm2, %%xmm3\n\t"
        "mulss %%xmm0, %%xmm3\n\t"
        "movss %%xmm3, %0"
        : "=m"(result)
        : "m"(x), "m"(inv_pi), "m"(one),
          "m"(c1),
          "m"(c2),
          "m"(c3),
          "m"(FLOAT_ONE)
        : "xmm0", "xmm1", "xmm2", "xmm3", "memory"
    );
    return result;
}

static inline f32 FloatCos(f32 x) {
    return FloatSin(x + M_HALF_PI);
}

static inline f64 DoubleExp(f64 x) {
    f64 result;
    const f64 one = 1.0;
    const f64 log2e = LOG2E;
    const f64 ln2 = LN2;
    
    __asm__ volatile (
        "movsd      %1,       %%xmm0\n\t"
        "mulsd      %2,       %%xmm0\n\t"
        "movsd      %%xmm0,   %%xmm1\n\t"
        "roundsd    $0x08,    %%xmm1, %%xmm1\n\t"
        "subsd      %%xmm1,   %%xmm0\n\t"
        "cvttsd2si  %%xmm1,   %%rax\n\t"
        "mulsd      %3,       %%xmm0\n\t"
        "movsd      %%xmm0,   %%xmm1\n\t"
        "mulsd      %%xmm1,   %%xmm1\n\t"
        "movsd      %4,       %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %5,       %%xmm2\n\t"
        "mulsd      %%xmm0,   %%xmm2\n\t"
        "addsd      %6,       %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %7,       %%xmm2\n\t"
        "mulsd      %%xmm0,   %%xmm2\n\t"
        "addsd      %8,       %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %9,       %%xmm2\n\t"
        "mulsd      %%xmm0,   %%xmm2\n\t"
        "addsd      %10,      %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %11,      %%xmm2\n\t"
        "addsd      %%xmm0,   %%xmm2\n\t"
        "addq       $1023,    %%rax\n\t"
        "salq       $52,      %%rax\n\t"
        "movq       %%rax,    %%xmm3\n\t"
        "mulsd      %%xmm3,   %%xmm2\n\t"
        "movsd      %%xmm2,   %0\n\t"
        : "=m"(result)
        : "m"(x), "m"(log2e), "m"(ln2),
          "m"(CONST_1_OVER_40320),
          "m"(CONST_1_OVER_5040),
          "m"(CONST_1_OVER_720),
          "m"(CONST_1_OVER_120),
          "m"(CONST_1_OVER_24),
          "m"(CONST_1_OVER_6),
          "m"(CONST_0_5),
          "m"(one)
        : "rax", "xmm0", "xmm1", "xmm2", "xmm3", "memory"
    );
    return result;
}

static inline void DoubleExp4(const f64* x, f64* result) {
    const f64 log2e = LOG2E;
    const f64 ln2 = LN2;
    
    __asm__ volatile (
        "vmovupd %1, %%ymm0\n\t"
        "vbroadcastsd %2, %%ymm1\n\t"
        "vmulpd %%ymm1, %%ymm0, %%ymm0\n\t"
        "vroundpd $0x09, %%ymm0, %%ymm1\n\t"
        "vsubpd %%ymm1, %%ymm0, %%ymm0\n\t"
        "vbroadcastsd %3, %%ymm1\n\t"
        "vmulpd %%ymm1, %%ymm0, %%ymm0\n\t"
        
        "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
        
        "vbroadcastsd %4, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %5, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %6, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %7, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %8, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %9, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %10, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %11, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        
        "vbroadcastsd %11, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vaddpd %%ymm0, %%ymm2, %%ymm0\n\t"
        
        "vmovupd %%ymm0, %0"
        : "=m"(result[0])
        : "m"(x[0]), "m"(log2e), "m"(ln2),
          "m"(CONST_1_OVER_5040),
          "m"(CONST_1_OVER_720),
          "m"(CONST_1_OVER_120),
          "m"(CONST_1_OVER_24),
          "m"(CONST_1_OVER_6),
          "m"(CONST_0_5),
          "m"(CONST_1),
          "m"(DOUBLE_ONE)
        : "ymm0", "ymm1", "ymm2", "ymm3", "memory"
    );
}

static inline f64 DoubleLog(f64 x) {
    if (x <= 0.0) return -1.0 / 0.0;
    
    union { f64 d; u64 u; } v = { .d = x };
    int exp = (int)((v.u >> 52) & 0x7FF) - 1023;
    v.u = (v.u & 0x800FFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL;
    f64 m = v.d;
    
    f64 z = (m - 1.0) / (m + 1.0);
    f64 z2 = z * z;
    f64 result;
    const f64 two = 2.0;
    const f64 ln2 = LN2;
    
    __asm__ volatile (
        "movsd      %1,       %%xmm0\n\t"
        "movsd      %2,       %%xmm1\n\t"
        "movsd      %3,       %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %4,       %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %5,       %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %6,       %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %7,       %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %8,       %%xmm2\n\t"
        "mulsd      %%xmm1,   %%xmm2\n\t"
        "addsd      %9,       %%xmm2\n\t"
        "mulsd      %%xmm0,   %%xmm2\n\t"
        "addsd      %%xmm2,   %%xmm0\n\t"
        "mulsd      %9,       %%xmm0\n\t"
        "movsd      %10,      %%xmm1\n\t"
        "cvtsi2sd   %11,      %%xmm2\n\t"
        "mulsd      %%xmm2,   %%xmm1\n\t"
        "addsd      %%xmm1,   %%xmm0\n\t"
        "movsd      %%xmm0,   %0\n\t"
        : "=m"(result)
        : "m"(z), "m"(z2),
          "m"(CONST_1_OVER_3),
          "m"(CONST_1_OVER_5),
          "m"(CONST_1_OVER_7),
          "m"(CONST_1_OVER_9),
          "m"(CONST_1_OVER_11),
          "m"(CONST_1_OVER_13),
          "m"(ln2),
          "m"(two),
          "r"(exp)
        : "xmm0", "xmm1", "xmm2", "memory"
    );
    return result;
}

static inline f64 DoubleTanh(f64 x) {
    f64 ax = DoubleAbs(x);
    if (ax > 19.0) return (x > 0) ? 1.0 : -1.0;
    
    f64 result;
    const f64 half = 0.5;
    const f64 third = 1.0/3.0;
    
    __asm__ volatile (
        "movsd %1, %%xmm0\n\t"
        "movsd %2, %%xmm1\n\t"
        "movsd %3, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "movsd %5, %%xmm3\n\t"
        "addsd %%xmm2, %%xmm3\n\t"
        "movsd %4, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "movsd %5, %%xmm4\n\t"
        "addsd %%xmm1, %%xmm4\n\t"
        "addsd %%xmm2, %%xmm4\n\t"
        "divsd %%xmm4, %%xmm3\n\t"
        "mulsd %%xmm0, %%xmm3\n\t"
        "movsd %%xmm3, %0"
        : "=m"(result)
        : "m"(x), "m"(ax),
          "m"(half),
          "m"(third),
          "m"(DOUBLE_ONE)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "memory"
    );
    return result;
}

static inline f64 DoubleAtan(f64 x) {
    f64 ax = DoubleAbs(x);
    int inverted = 0;
    if (ax > 1.0) {
        ax = 1.0 / ax;
        inverted = 1;
    }
    
    f64 ax2 = ax * ax;
    f64 result;
    
    __asm__ volatile (
        "movsd %1, %%xmm0\n\t"
        "movsd %2, %%xmm1\n\t"
        "movsd %3, %%xmm2\n\t"
        "movsd %4, %%xmm3\n\t"
        "movsd %5, %%xmm4\n\t"
        "movsd %6, %%xmm5\n\t"
        "movsd %7, %%xmm6\n\t"
        "movsd %8, %%xmm7\n\t"
        
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %4, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %5, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %6, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %7, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %8, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "movsd %9, %%xmm3\n\t"
        "addsd %%xmm3, %%xmm2\n\t"
        "mulsd %%xmm0, %%xmm2\n\t"
        "movsd %%xmm2, %0"
        : "=m"(result)
        : "m"(ax), "m"(ax2),
          "m"(CONST_1_OVER_13),
          "m"(CONST_1_OVER_11),
          "m"(CONST_1_OVER_9),
          "m"(CONST_1_OVER_7),
          "m"(CONST_1_OVER_5),
          "m"(CONST_1_OVER_3),
          "m"(DOUBLE_ONE)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory"
    );
    
    if (inverted) result = M_HALF_PI - result;
    if (x < 0) result = -result;
    return result;
}

static inline f64 DoubleAtan2(f64 y, f64 x) {
    if (x == 0.0) {
        if (y > 0) return M_HALF_PI;
        if (y < 0) return -M_HALF_PI;
        return 0.0;
    }
    if (y == 0.0) {
        if (x > 0) return 0.0;
        return M_PI;
    }
    
    f64 result = DoubleAtan(y / x);
    if (x < 0) {
        if (y >= 0) result += M_PI;
        else result -= M_PI;
    }
    return result;
}

static inline f64 DoubleSqrt(f64 x) {
    if (x <= 0.0) return 0.0;
    f64 result;
    __asm__ volatile (
        "sqrtsd %1, %%xmm0\n\t"
        "movsd %%xmm0, %0"
        : "=m"(result)
        : "m"(x)
        : "xmm0"
    );
    return result;
}

static inline f64 DoubleSinCos(f64 x, f64* cos_out) {
    x = DoubleMod2PI(x);
    
    f64 sign_sin = 1.0;
    f64 sign_cos = 1.0;
    if (x < 0) {
        x = -x;
        sign_sin = -sign_sin;
    }
    if (x > M_PI) {
        x = M_2PI - x;
        sign_sin = -sign_sin;
        sign_cos = -sign_cos;
    }
    if (x > M_HALF_PI) {
        x = M_PI - x;
        sign_cos = -sign_cos;
    }
    
    f64 x2 = x * x;
    f64 sin_res, cos_res;
    
    __asm__ volatile (
        "movsd %1, %%xmm0\n\t"
        "movsd %2, %%xmm1\n\t"
        "movsd %3, %%xmm2\n\t"
        "movsd %4, %%xmm3\n\t"
        "movsd %5, %%xmm4\n\t"
        "movsd %6, %%xmm5\n\t"
        "movsd %7, %%xmm6\n\t"
        "movsd %8, %%xmm7\n\t"
        "movsd %9, %%xmm8\n\t"
        "movsd %10, %%xmm9\n\t"
        
        "movsd %%xmm1, %%xmm10\n\t"
        "movsd %%xmm1, %%xmm11\n\t"
        
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %4, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %5, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %6, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %7, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "addsd %8, %%xmm2\n\t"
        "mulsd %%xmm1, %%xmm2\n\t"
        "movsd %19, %%xmm3\n\t"
        "addsd %%xmm3, %%xmm2\n\t"
        "mulsd %%xmm0, %%xmm2\n\t"
        "movsd %%xmm2, %11\n\t"
        
        "mulsd %%xmm1, %%xmm9\n\t"
        "addsd %13, %%xmm9\n\t"
        "mulsd %%xmm1, %%xmm9\n\t"
        "addsd %14, %%xmm9\n\t"
        "mulsd %%xmm1, %%xmm9\n\t"
        "addsd %15, %%xmm9\n\t"
        "mulsd %%xmm1, %%xmm9\n\t"
        "addsd %16, %%xmm9\n\t"
        "mulsd %%xmm1, %%xmm9\n\t"
        "addsd %17, %%xmm9\n\t"
        "mulsd %%xmm1, %%xmm9\n\t"
        "movsd %19, %%xmm3\n\t"
        "addsd %%xmm3, %%xmm9\n\t"
        "movsd %%xmm9, %12"
        : "=m"(sin_res), "=m"(cos_res)
        : "m"(x), "m"(x2),
          "m"(CONST_1_OVER_479001600),
          "m"(CONST_1_OVER_39916800),
          "m"(CONST_1_OVER_3628800),
          "m"(CONST_1_OVER_362880),
          "m"(CONST_1_OVER_5040),
          "m"(CONST_1_OVER_120),
          "m"(CONST_1_OVER_6),
          "m"(CONST_1_OVER_87178291200),
          "m"(CONST_1_OVER_479001600),
          "m"(CONST_1_OVER_39916800),
          "m"(CONST_1_OVER_3628800),
          "m"(CONST_1_OVER_40320),
          "m"(CONST_1_OVER_720),
          "m"(CONST_1_OVER_24),
          "m"(CONST_0_5),
          "m"(DOUBLE_ONE)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "memory"
    );
    
    *cos_out = sign_cos * cos_res;
    return sign_sin * sin_res;
}

static inline void DoubleSin4(const f64* x, f64* result) {
    const f64 inv_2pi = INV_2PI;
    const f64 two_pi = M_2PI;
    const f64 c1 = 0.16666666666666666666666666666666666666666666666667;
    const f64 c2 = 0.0083333333333333333333333333333333333333333333333333;
    const f64 c3 = 0.0001984126984126984126984126984126984126984126984127;
    const f64 c4 = 0.0000027557319223985890652557319223985890652557319223986;
    const f64 c5 = 0.000000025052108385441718775086526454433405762264671030183;
    const f64 c6 = 0.00000000016059043836821614599367051893912127525694291685921;
    
    __asm__ volatile (
        "vmovupd %1, %%ymm0\n\t"
        "vbroadcastsd %2, %%ymm1\n\t"
        "vmulpd %%ymm1, %%ymm0, %%ymm0\n\t"
        "vroundpd $0x09, %%ymm0, %%ymm0\n\t"
        "vbroadcastsd %3, %%ymm1\n\t"
        "vmulpd %%ymm1, %%ymm0, %%ymm0\n\t"
        "vmovupd %1, %%ymm1\n\t"
        "vsubpd %%ymm0, %%ymm1, %%ymm0\n\t"
        
        "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
        "vmulpd %%ymm0, %%ymm1, %%ymm2\n\t"
        "vbroadcastsd %4, %%ymm3\n\t"
        "vmulpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vsubpd %%ymm2, %%ymm0, %%ymm2\n\t"
        
        "vmulpd %%ymm1, %%ymm2, %%ymm3\n\t"
        "vbroadcastsd %5, %%ymm4\n\t"
        "vmulpd %%ymm4, %%ymm3, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        
        "vmulpd %%ymm1, %%ymm2, %%ymm3\n\t"
        "vbroadcastsd %6, %%ymm4\n\t"
        "vmulpd %%ymm4, %%ymm3, %%ymm3\n\t"
        "vsubpd %%ymm3, %%ymm2, %%ymm2\n\t"
        
        "vmulpd %%ymm1, %%ymm2, %%ymm3\n\t"
        "vbroadcastsd %7, %%ymm4\n\t"
        "vmulpd %%ymm4, %%ymm3, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        
        "vmulpd %%ymm1, %%ymm2, %%ymm3\n\t"
        "vbroadcastsd %8, %%ymm4\n\t"
        "vmulpd %%ymm4, %%ymm3, %%ymm3\n\t"
        "vsubpd %%ymm3, %%ymm2, %%ymm2\n\t"
        
        "vmulpd %%ymm1, %%ymm2, %%ymm3\n\t"
        "vbroadcastsd %9, %%ymm4\n\t"
        "vmulpd %%ymm4, %%ymm3, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm0\n\t"
        
        "vmovupd %%ymm0, %0"
        : "=m"(result[0])
        : "m"(x[0]), "m"(inv_2pi), "m"(two_pi),
          "m"(c1),
          "m"(c2),
          "m"(c3),
          "m"(c4),
          "m"(c5),
          "m"(c6)
        : "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "memory"
    );
}

static inline void DoubleCos4(const f64* x, f64* result) {
    const f64 inv_2pi = INV_2PI;
    const f64 two_pi = M_2PI;
    
    __asm__ volatile (
        "vmovupd %1, %%ymm0\n\t"
        "vbroadcastsd %2, %%ymm1\n\t"
        "vmulpd %%ymm1, %%ymm0, %%ymm0\n\t"
        "vroundpd $0x09, %%ymm0, %%ymm0\n\t"
        "vbroadcastsd %3, %%ymm1\n\t"
        "vmulpd %%ymm1, %%ymm0, %%ymm0\n\t"
        "vmovupd %1, %%ymm1\n\t"
        "vsubpd %%ymm0, %%ymm1, %%ymm0\n\t"
        
        "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
        
        "vbroadcastsd %4, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %5, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %6, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %7, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %8, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %9, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %10, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        "vbroadcastsd %11, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm2\n\t"
        "vmulpd %%ymm1, %%ymm2, %%ymm2\n\t"
        
        "vbroadcastsd %12, %%ymm3\n\t"
        "vaddpd %%ymm3, %%ymm2, %%ymm0\n\t"
        
        "vmovupd %%ymm0, %0"
        : "=m"(result[0])
        : "m"(x[0]), "m"(inv_2pi), "m"(two_pi),
          "m"(CONST_1_OVER_87178291200),
          "m"(CONST_1_OVER_479001600),
          "m"(CONST_1_OVER_39916800),
          "m"(CONST_1_OVER_3628800),
          "m"(CONST_1_OVER_40320),
          "m"(CONST_1_OVER_720),
          "m"(CONST_1_OVER_24),
          "m"(CONST_0_5),
          "m"(DOUBLE_ONE)
        : "ymm0", "ymm1", "ymm2", "ymm3", "memory"
    );
}

f64 DoubleRound(f64 val, int round_to) {
    f64 factor = 1.0;
    for (int i = 0; i < round_to; i++) {
        factor *= 10.0;
    }

    val *= factor;
    f64 result;

    __asm__ volatile (
        "vroundsd $0x04, %1, %1, %0\n\t"
        : "=x" (result)
        : "x" (val)
    );

    return result / factor;
}

f32 FloatRound(f32 val, int round_to) {
    static const f32 pow10[] = {
        1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f, 1000000.0f
    };
    
    if (round_to < 0 || round_to > 6) return val;
    
    f32 factor = pow10[round_to];
    
    f32 sign = (val < 0.0f) ? -1.0f : 1.0f;
    if (val < 0.0f) val = -val;

    val *= factor;

    val += 1e-6f; 

    f32 result;

    __asm__ volatile (
        "vroundss $0x04, %1, %1, %0\n\t"
        : "=x" (result)
        : "x" (val)
    );


    return (result / factor) * sign;
}