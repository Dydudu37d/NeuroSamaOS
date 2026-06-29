#pragma once

#include "int.h"

struct GDTEntry {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  granularity;
    u8  base_high;
} __attribute__((packed));

void LoadGDT();
