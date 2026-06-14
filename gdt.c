#include "gdt.h"

struct GDTR {
    u16 limit;
    u64 base;
} __attribute__((packed));

static struct GDTEntry gdt[3] = {
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0x9A, 0, 0},
    {0, 0, 0, 0x92, 0, 0}
};

static struct GDTR gdtr;

void LoadGDT() {
    gdt[1].limit_low = 0;
    gdt[1].base_low = 0;
    gdt[1].base_mid = 0;
    gdt[1].access = 0x9A;
    gdt[1].granularity = 0x20;
    gdt[1].base_high = 0;
    
    gdt[2].limit_low = 0;
    gdt[2].base_low = 0;
    gdt[2].base_mid = 0;
    gdt[2].access = 0x92;
    gdt[2].granularity = 0x20;
    gdt[2].base_high = 0;
    
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (u64)gdt;
    
    asm volatile(
        "lgdt %0\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        :
        : "m"(gdtr)
        : "rax", "memory"
    );
}
