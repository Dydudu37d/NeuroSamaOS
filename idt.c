#include "idt.h"
#include "debug.h"
#include "Context.h"

extern RegContext MainGlobalContext;

static struct IDTEntry idt[256];
static struct IDTR idtr;

volatile u64 LastErrorCode = 0;
volatile u64 LastVector = 0;
volatile _Bool IDTGotError = 0;

struct InterruptFrame {
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rbp;
} __attribute__((packed));

struct InterruptFrame frame = {0};

void IDTCloseError(void) {
    IDTGotError = 0;
}

u8 get_instruction_length(u64 rip) {
    u8 first_byte = *(u8*)rip;

    if (first_byte == 0x0f) return 2;
    if ((first_byte & 0xf0) == 0x40) return 2;
    if ((first_byte & 0xfe) == 0xf0) return 1;

    switch(first_byte) {
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57:
        case 0x58: case 0x59: case 0x5a: case 0x5b:
        case 0x5c: case 0x5d: case 0x5e: case 0x5f:
        case 0x90: case 0xf4: case 0xc3: case 0xcc:
            return 1;
        case 0xcd: case 0xeb:
            return 2;
        case 0xe8: case 0xe9:
            return 5;
        case 0x74: case 0x75: case 0x7c: case 0x7d:
        case 0x7e: case 0x7f:
            return 2;
        default:
            return 3;
    }
}

void handle_exception_fast(u64 vector, u64 error_code, u64 *rip, u64 *cs, u64 *rflags) {
    LastVector = vector;
    LastErrorCode = error_code;
    IDTGotError = 1;
    
    DebugStr("Exception: ");
    DebugU64(vector);
    if (vector == 8 || vector == 10 || vector == 11 || vector == 12 || vector == 13 || vector == 14 || vector == 17 || vector == 21) {
        DebugStr(" Error code: ");
        DebugU64(error_code);
    }
    DebugStr(" RIP=");
    DebugU64(*rip);
    DebugChar('\n');
    
    if (vector == 7) {
        *rip += 2;
    } else {
        u8 len = get_instruction_length(*rip);
        *rip += len;
    }
}

#define EXC_HANDLER_NO_ERROR(vector) \
__attribute__((naked)) void exception_handler_##vector(void) { \
    asm volatile( \
        "push %%rbp\n" \
        "mov %%rsp, %%rbp\n" \
        "mov %0, %%rdi\n" \
        "xor %%rsi, %%rsi\n" \
        "lea 8(%%rbp), %%rdx\n" \
        "lea 16(%%rbp), %%rcx\n" \
        "lea 24(%%rbp), %%r8\n" \
        "call handle_exception_fast\n" \
        "pop %%rbp\n" \
        "iretq\n" \
        : : "i"(vector) : "rax", "rdi", "rsi", "rdx", "rcx", "r8", "memory" \
    ); \
}

#define EXC_HANDLER_WITH_ERROR(vector) \
__attribute__((naked)) void exception_handler_##vector(u64 error_code) { \
    asm volatile( \
        "push %%rbp\n" \
        "mov %%rsp, %%rbp\n" \
        "mov %0, %%rdi\n" \
        "mov 8(%%rbp), %%rsi\n" \
        "lea 16(%%rbp), %%rdx\n" \
        "lea 24(%%rbp), %%rcx\n" \
        "lea 32(%%rbp), %%r8\n" \
        "call handle_exception_fast\n" \
        "pop %%rbp\n" \
        "add $8, %%rsp\n" \
        "iretq\n" \
        : : "i"(vector) : "rax", "rdi", "rsi", "rdx", "rcx", "r8", "memory" \
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
        SetIDTEntry(i, 0, 0, 0);
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

    DebugStr("IDT loaded\n");
}
