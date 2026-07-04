#pragma once
#include "int.h"

#define IA32_MCG_STATUS 0x17A
#define IA32_MCG_CAP    0x17B
#define IA32_MC0_STATUS 0x401
#define IA32_MC0_ADDR   0x402
#define IA32_MC0_MISC   0x403
#define MC_STATUS_UC   (1ULL << 61)
#define MC_STATUS_EN   (1ULL << 60)
#define MC_STATUS_PCC  (1ULL << 58)
#define MC_STATUS_VALID (1ULL << 63)

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
void InitInterruptSystem();
void IDTCloseError(void);
