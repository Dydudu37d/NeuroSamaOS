#pragma once
#include "int.h"

struct IDTEntry {
    u16 offset_1;
    u16 selector;
    u8  ist;
    u8  type_attr;
    u16 offset_2;
    u32 offset_3;
    u32 zero;
} __attribute__((packed));

struct IDTR {
    u16 limit;
    u64 base;
} __attribute__((packed));

void InitIDT();
void IDTCloseError(void);
