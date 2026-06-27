#pragma once

#include "int.h"

#define M_PI 3.14159265358979323846L
#define M_2PI 6.28318530717958647692L
#define M_HALF_PI 1.57079632679489661923L

static inline s64 S64Abs(s64 x){
    return x&0x7FFFFFFFFFFFFFFFULL;
}

static inline u64 U64Pow(u64 x,u64 time){
    return x<<time;
}

static inline double DoublePow(double x, u64 time) {
    double result = 1.0;
    double base = x;
    while (time > 0) {
        if (time & 1) result *= base;
        base *= base;
        time >>= 1;
    }
    return result;
}

static inline double DoubleMod(double x, double y) {
    if (!y || !x) return 0.0;
    union { double d; u64 u; } vx = { .d = x }, vy = { .d = y };
    u64 sx = vx.u & 0x8000000000000000ULL;
    vx.u &= 0x7FFFFFFFFFFFFFFFULL;
    vy.u &= 0x7FFFFFFFFFFFFFFFULL;
    
    int exp_x = (int)((vx.u >> 52) & 0x7FF);
    int exp_y = (int)((vy.u >> 52) & 0x7FF);
    u64 mant_x = vx.u & 0x000FFFFFFFFFFFFFULL;
    u64 mant_y = vy.u & 0x000FFFFFFFFFFFFFULL;
    
    u64 less = (exp_x < exp_y) | ((exp_x == exp_y) & (mant_x < mant_y));
    u64 less_mask = (u64)(-less);
    
    double q = vx.d / vy.d;
    union { double d; u64 u; } vq = { .d = q };
    int exp_q = (int)((vq.u >> 52) & 0x7FF) - 1023;
    
    u64 safe = (exp_q < 53);
    u64 safe_mask = (u64)(-safe);
    
    s64 n = (s64)q;
    u64 q_sign = vq.u & 0x8000000000000000ULL;
    u64 not_int = (q != (double)n);
    u64 neg_adj_mask = q_sign & (not_int ? ~0ULL : 0);
    n -= (s64)neg_adj_mask;
    
    double r = vx.d - (double)n * vy.d;
    
    for (int i = 0; i < 3; i++) {
        r = (r >= vy.d) ? (r - vy.d) : r;
        r = (r < 0.0) ? (r + vy.d) : r;
    }
    
    double result = less ? vx.d : r;
    union { double d; u64 u; } vr = { .d = result };
    vr.u |= sx;
    return vr.d;
}

static inline double DoubleAbs(double x) {
    union { double d; u64 u; } v = { .d = x };
    v.u &= 0x7FFFFFFFFFFFFFFFULL;
    return v.d;
}

static inline double DoubleSin(double x) {
    x = DoubleMod(x, M_2PI);
    
    double sign = 1.0;
    if (x < 0) {
        x = -x;
        sign = -sign;
    }
    if (x > M_PI) {
        x = M_2PI - x;
        sign = -sign;
    }
    if (x > M_HALF_PI) {
        x = M_PI - x;
    }
    
    double ret = x;
    double term = x;
    double x2 = x * x;
    double s = -1.0;
    
    for (u64 n = 3; n <= 41; n += 2) {
        term *= x2 / ((double)(n - 1) * n);
        ret += s * term;
        s = -s;
    }
    
    return sign * ret;
}

static inline double DoubleCos(double x) {
    x = DoubleMod(x, M_2PI);
    
    double sign = 1.0;
    if (x < 0) {
        x = -x;
        sign = -sign;
    }
    if (x > M_PI) {
        x = M_2PI - x;
        sign = -sign;
    }
    if (x > M_HALF_PI) {
        x = M_PI - x;
    }
    
    double ret = 1.0;
    double term = 1.0;
    double x2 = x * x;
    double s = -1.0;
    
    for (u64 n = 2; n <= 40; n += 2) {
        term *= x2 / ((double)(n - 1) * n);
        ret += s * term;
        s = -s;
    }
    
    return sign * ret;
}

static inline float FloatSin(float x) {
    union { float f; u32 u; } v;
    const float INV_PI = 0.31830988618379067154f;
    const u32 MAGIC = 0x3F800000;
    v.f = x * INV_PI;
    u32 i = v.u;
    v.u = (i & 0x807FFFFF) | MAGIC;
    float frac = v.f - 1.0f;
    if (i & 0x80000000) frac = -frac;
    float angle = frac * 3.141592653589793f;
    float a = angle * angle;
    return angle * (1.0f - a * (0.16666666666666666f - a * (0.008333333333333333f - a * 0.0001984126984126984f)));
}

static inline float FloatCos(float x) {
    return FloatSin(x + M_HALF_PI);
}

static inline double DoubleExp(double x) {
    union { double d; u64 u; } v;
    const u64 MAGIC = 0x3FF71547652B82FEULL;
    v.u = MAGIC + (u64)(x * (double)MAGIC);
    double y = v.d;
    double t = y - 1.0;
    double log_y = t - t*t/2.0 + t*t*t/3.0;
    return y * (1.0 + x - log_y);
}

static inline double DoubleLog(double x) {
    if (x <= 0.0) return -1.0 / 0.0;
    
    union { double d; u64 u; } v = { .d = x };
    int exp = (int)((v.u >> 52) & 0x7FF) - 1023;
    v.u = (v.u & 0x800FFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL;
    double m = v.d;
    
    double t = m - 1.0;
    double guess = t - t*t/2.0 + t*t*t/3.0 - t*t*t*t/4.0 + t*t*t*t*t/5.0;
    
    for (int i = 0; i < 4; i++) {
        double e = DoubleExp(guess);
        guess = guess - (e - m) / e;
    }
    
    return guess + (double)exp * 0.6931471805599453;
}

static inline double DoubleTanh(double x) {
    double ax = S64Abs(x);
    return x * (1.0 + 0.5 * ax) / (1.0 + ax + 0.3333333333333333 * ax * ax);
}
