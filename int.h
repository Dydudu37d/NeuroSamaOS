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

typedef struct{
    u64 low;
    u64 high;
} __attribute__((packed)) u128;

typedef struct{
    s64 low;
    s64 high;
} __attribute__((packed)) s128;

typedef struct{
    double low;
    double high;
} __attribute__((packed))  dd;

typedef struct{
    float low;
    float high;
} __attribute__((packed)) ff;
