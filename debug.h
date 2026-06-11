#pragma once

#include "port.h"

static inline void DebugInit(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x01);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
    inb(0x3F8 + 2);
    inb(0x3F8 + 0);
}

static inline void DebugChar(const char C) {
    outb(0x3F8, C);
}

void DebugStr(const char* S);
void DebugU64(u64 LLU);
void DebugU64Bit(u64 LLU);
