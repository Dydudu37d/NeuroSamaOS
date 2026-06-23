#include "idt.h"
#include "debug.h"
#include "Context.h"

extern RegContext MainGlobalContext;

static struct IDTEntry idt[256];
static struct IDTR idtr;

struct TSS {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iopb_offset;
} __attribute__((packed));

volatile u64 LastErrorCode = 0;
volatile u64 LastVector = 0;
volatile _Bool IDTGotError = 0;
volatile _Bool InExceptionHandler = 0;
volatile static u64 Cr2;
volatile static u64 Cr3;

struct InterruptFrame {
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} __attribute__((packed));

void IDTCloseError(void) {
    IDTGotError = 0;
}

static void outb_hex64(u64 val) {
    const char hex[] = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4) {
        outb(0x3F8, hex[(val >> i) & 0xF]);
    }
}

static void outb_str(const char* s) {
    while (*s) outb(0x3F8, *s++);
}

u8 get_instruction_length(u64 rip) {
    if (rip < 0x1000) return 1;
    u8 first_byte = *(volatile u8*)rip;
    if (first_byte == 0x0F) {
        u8 second_byte = *(volatile u8*)(rip + 1);
        if (second_byte == 0x38 || second_byte == 0x3A) return 3;
        return 2;
    }
    if ((first_byte & 0xF0) == 0x40) return 2;
    switch(first_byte) {
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57:
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        case 0x90: case 0xC3: case 0xCC: case 0xF4:
            return 1;
        case 0xCD: case 0xEB:
            return 2;
        case 0xE8: case 0xE9:
            return 5;
        case 0x74: case 0x75: case 0x7C: case 0x7D:
        case 0x7E: case 0x7F:
            return 2;
        default:
            return 3;
    }
}

void handle_exception_fast(u64 vector, u64 error_code, u64 *rip, u64 *cs, u64 *rflags) {
    if (InExceptionHandler) {
        outb_str("\nRECURSIVE EXCEPTION HALT\n");
        while(1) { asm volatile("cli; hlt"); }
    }
    InExceptionHandler = 1;
    
    LastVector = vector;
    LastErrorCode = error_code;
    IDTGotError = 1;
    asm volatile("movq %%cr2,%0 \n\t" : "=r"(Cr2) : : "memory");
    asm volatile("movq %%cr3,%0 \n\t" : "=r"(Cr3) : : "memory");

    outb_str("\nEXC V=");
    outb_hex64(vector);
    outb_str(" E=");
    outb_hex64(error_code);
    outb_str(" RIP=");
    outb_hex64(*rip);
    outb_str(" CS=");
    outb_hex64(*cs);
    outb_str(" CR2=");
    outb_hex64(Cr2);
    outb_str(" CR3=");
    outb_hex64(Cr3);
    outb_str("\n");

    if (vector == 8) {
        outb_str("DF HALT\n");
        while(1) { asm volatile("cli; hlt"); }
    }

    if (*rip != 0) {
        u8 len = get_instruction_length(*rip);
        *rip += len;
    } else {
        outb_str("NULL RIP HALT\n");
        while(1) { asm volatile("cli; hlt"); }
    }
    
    InExceptionHandler = 0;
}

#define EXC_HANDLER_NO_ERROR(vector) \
__attribute__((naked)) void exception_handler_##vector(void) { \
    asm volatile( \
        "pushq $0\n\t" \
        "pushq %%rax\n\t" "pushq %%rcx\n\t" "pushq %%rdx\n\t" "pushq %%rbx\n\t" \
        "pushq %%rbp\n\t" "pushq %%rsi\n\t" "pushq %%rdi\n\t" \
        "pushq %%r8\n\t"  "pushq %%r9\n\t"  "pushq %%r10\n\t" "pushq %%r11\n\t" \
        "pushq %%r12\n\t" "pushq %%r13\n\t" "pushq %%r14\n\t" "pushq %%r15\n\t" \
        "movq $" #vector ", %%rcx\n\t"\
        "movq 0(%%rsp), %%rdx\n\t" \
        "leaq 128(%%rsp), %%r8\n\t" \
        "leaq 136(%%rsp), %%r9\n\t" \
        "leaq 144(%%rsp), %%rax\n\t" \
        "call handle_exception_fast\n\t" \
        "popq %%r15\n\t" "popq %%r14\n\t" "popq %%r13\n\t" "popq %%r12\n\t" \
        "popq %%r11\n\t" "popq %%r10\n\t" "popq %%r9\n\t"  "popq %%r8\n\t" \
        "popq %%rdi\n\t" "popq %%rsi\n\t" "popq %%rbp\n\t" "popq %%rbx\n\t" \
        "popq %%rdx\n\t" "popq %%rcx\n\t" "popq %%rax\n\t" \
        "addq $8, %%rsp\n\t" \
        "iretq\n\t" \
        ::: "memory" \
    ); \
}

#define EXC_HANDLER_WITH_ERROR(vector) \
__attribute__((naked)) void exception_handler_##vector(u64 error_code) { \
    asm volatile( \
        "pushq %%rax\n\t" "pushq %%rcx\n\t" "pushq %%rdx\n\t" "pushq %%rbx\n\t" \
        "pushq %%rbp\n\t" "pushq %%rsi\n\t" "pushq %%rdi\n\t" \
        "pushq %%r8\n\t"  "pushq %%r9\n\t"  "pushq %%r10\n\t" "pushq %%r11\n\t" \
        "pushq %%r12\n\t" "pushq %%r13\n\t" "pushq %%r14\n\t" "pushq %%r15\n\t" \
        "movq $" #vector ", %%rcx\n\t" \
        "movq 120(%%rsp), %%rdx\n\t" \
        "leaq 128(%%rsp), %%r8\n\t" \
        "leaq 136(%%rsp), %%r9\n\t" \
        "leaq 144(%%rsp), %%rax\n\t" \
        "call handle_exception_fast\n\t" \
        "popq %%r15\n\t" "popq %%r14\n\t" "popq %%r13\n\t" "popq %%r12\n\t" \
        "popq %%r11\n\t" "popq %%r10\n\t" "popq %%r9\n\t"  "popq %%r8\n\t" \
        "popq %%rdi\n\t" "popq %%rsi\n\t" "popq %%rbp\n\t" "popq %%rbx\n\t" \
        "popq %%rdx\n\t" "popq %%rcx\n\t" "popq %%rax\n\t" \
        "addq $8, %%rsp\n\t" \
        "iretq\n\t" \
        ::: "memory" \
    ); \
}

EXC_HANDLER_NO_ERROR(0)
EXC_HANDLER_NO_ERROR(1)
EXC_HANDLER_NO_ERROR(2)
EXC_HANDLER_NO_ERROR(3)
EXC_HANDLER_NO_ERROR(4)
EXC_HANDLER_NO_ERROR(5)
EXC_HANDLER_NO_ERROR(6)
EXC_HANDLER_NO_ERROR(7)
EXC_HANDLER_WITH_ERROR(8)
EXC_HANDLER_NO_ERROR(9)
EXC_HANDLER_WITH_ERROR(10)
EXC_HANDLER_WITH_ERROR(11)
EXC_HANDLER_WITH_ERROR(12)
EXC_HANDLER_WITH_ERROR(13)
EXC_HANDLER_WITH_ERROR(14)
EXC_HANDLER_NO_ERROR(15)
EXC_HANDLER_NO_ERROR(16)
EXC_HANDLER_WITH_ERROR(17)
EXC_HANDLER_NO_ERROR(18)
EXC_HANDLER_NO_ERROR(19)
EXC_HANDLER_NO_ERROR(20)
EXC_HANDLER_WITH_ERROR(21)
EXC_HANDLER_NO_ERROR(22)
EXC_HANDLER_NO_ERROR(23)
EXC_HANDLER_NO_ERROR(24)
EXC_HANDLER_NO_ERROR(25)
EXC_HANDLER_NO_ERROR(26)
EXC_HANDLER_NO_ERROR(27)
EXC_HANDLER_NO_ERROR(28)
EXC_HANDLER_NO_ERROR(29)
EXC_HANDLER_NO_ERROR(30)
EXC_HANDLER_NO_ERROR(31)

#define APIC_EOI_REG ((volatile u32*)(0xFEE00000ULL + 0xB0))

__attribute__((interrupt))
void default_hardware_interrupt_handler(struct InterruptFrame* frame) {
    
}

void SetIDTEntry(int n, u64 handler, u16 selector, u8 type_attr) {
    idt[n].offset_1 = handler & 0xFFFF;
    idt[n].selector = selector;
    idt[n].ist = 0;
    idt[n].type_attr = type_attr;
    idt[n].offset_2 = (handler >> 16) & 0xFFFF;
    idt[n].offset_3 = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

void InitIDT() {
    for (int i = 0; i < 256; i++) {
        SetIDTEntry(i, (u64)default_hardware_interrupt_handler, 0x08, 0x8E);
    }

    SetIDTEntry(0,  (u64)exception_handler_0, 0x08, 0x8E);
    SetIDTEntry(1,  (u64)exception_handler_1, 0x08, 0x8E);
    SetIDTEntry(2,  (u64)exception_handler_2, 0x08, 0x8E);
    SetIDTEntry(3,  (u64)exception_handler_3, 0x08, 0x8E);
    SetIDTEntry(4,  (u64)exception_handler_4, 0x08, 0x8E);
    SetIDTEntry(5,  (u64)exception_handler_5, 0x08, 0x8E);
    SetIDTEntry(6,  (u64)exception_handler_6, 0x08, 0x8E);
    SetIDTEntry(7,  (u64)exception_handler_7, 0x08, 0x8E);
    SetIDTEntry(8,  (u64)exception_handler_8, 0x08, 0x8E);
    SetIDTEntry(9,  (u64)exception_handler_9, 0x08, 0x8E);
    SetIDTEntry(10, (u64)exception_handler_10, 0x08, 0x8E);
    SetIDTEntry(11, (u64)exception_handler_11, 0x08, 0x8E);
    SetIDTEntry(12, (u64)exception_handler_12, 0x08, 0x8E);
    SetIDTEntry(13, (u64)exception_handler_13, 0x08, 0x8E);
    SetIDTEntry(14, (u64)exception_handler_14, 0x08, 0x8E);
    SetIDTEntry(15, (u64)exception_handler_15, 0x08, 0x8E);
    SetIDTEntry(16, (u64)exception_handler_16, 0x08, 0x8E);
    SetIDTEntry(17, (u64)exception_handler_17, 0x08, 0x8E);
    SetIDTEntry(18, (u64)exception_handler_18, 0x08, 0x8E);
    SetIDTEntry(19, (u64)exception_handler_19, 0x08, 0x8E);
    SetIDTEntry(20, (u64)exception_handler_20, 0x08, 0x8E);
    SetIDTEntry(21, (u64)exception_handler_21, 0x08, 0x8E);
    SetIDTEntry(22, (u64)exception_handler_22, 0x08, 0x8E);
    SetIDTEntry(23, (u64)exception_handler_23, 0x08, 0x8E);
    SetIDTEntry(24, (u64)exception_handler_24, 0x08, 0x8E);
    SetIDTEntry(25, (u64)exception_handler_25, 0x08, 0x8E);
    SetIDTEntry(26, (u64)exception_handler_26, 0x08, 0x8E);
    SetIDTEntry(27, (u64)exception_handler_27, 0x08, 0x8E);
    SetIDTEntry(28, (u64)exception_handler_28, 0x08, 0x8E);
    SetIDTEntry(29, (u64)exception_handler_29, 0x08, 0x8E);
    SetIDTEntry(30, (u64)exception_handler_30, 0x08, 0x8E);
    SetIDTEntry(31, (u64)exception_handler_31, 0x08, 0x8E);

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u64)&idt;
    asm volatile("lidt %0" : : "m"(idtr) : "memory");
}

void InitInterruptSystem() {
    InitIDT();
}
