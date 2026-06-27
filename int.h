#pragma once

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

#define NULL ((void*)0)

#define true 1
#define false 0

#define ALIGN_UP(val, align)   (((val) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(val, align) ((val) & ~((align) - 1))
#define PTR_ALIGN_UP(ptr, align) ((void*)ALIGN_UP((u64)(ptr), (align)))

inline _Bool IsAligned(void* ptr,u8 Aligned) {
    return ((((u64)ptr)) & Aligned) == 0;
}
