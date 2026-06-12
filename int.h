#pragma once

#include <immintrin.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

typedef __SIZE_TYPE__ size_t;

typedef __builtin_va_list __gnuc_va_list;
typedef __gnuc_va_list va_list;

typedef __m128i u128;
typedef __m128i s128;

#define NULL ((void*)0)

#define true 1
#define false 0

#define ALIGN_UP(val, align)   (((val) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(val, align) ((val) & ~((align) - 1))
#define PTR_ALIGN_UP(ptr, align) ((void*)ALIGN_UP((u64)(ptr), (align)))
#define LOAD_128(ptr) _mm_load_si128((const u128*)(ptr))
#define LOAD_128_UNALIGNED(ptr) _mm_loadu_si128((const u128*)(ptr))

inline _Bool IsAligned(void* ptr,u8 Aligned) {
    return ((((u64)ptr)) & Aligned) == 0;
}
