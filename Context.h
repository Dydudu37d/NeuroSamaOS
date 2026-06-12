#pragma  once

#include "int.h"

typedef struct{
    u64 Reg0t15[16];
    u64 cs, ds, es, fs, gs, ss;
    u64 rflags;
    u8 SIMDBuffer[2048] __attribute__((aligned(64)));
} RegContext;

static inline void SaveContext(RegContext *ctx) {
    asm volatile(
        "mov %%rax, %0\n"
        "mov %%rcx, %1\n"
        "mov %%rdx, %2\n"
        "mov %%rbx, %3\n"
        "mov %%rsp, %4\n"
        "mov %%rbp, %5\n"
        "mov %%rsi, %6\n"
        "mov %%rdi, %7\n"
        "mov %%r8,  %8\n"
        "mov %%r9,  %9\n"
        "mov %%r10, %10\n"
        "mov %%r11, %11\n"
        "mov %%r12, %12\n"
        "mov %%r13, %13\n"
        "mov %%r14, %14\n"
        "mov %%r15, %15\n"
        "mov %%cs, %16\n"
        "mov %%ds, %17\n"
        "mov %%es, %18\n"
        "mov %%fs, %19\n"
        "mov %%gs, %20\n"
        "mov %%ss, %21\n"
        "pushfq\n"
        "popq %22\n"
        "xsave %23\n"
        : "=m"(ctx->Reg0t15[0]),  "=m"(ctx->Reg0t15[1]),
          "=m"(ctx->Reg0t15[2]),  "=m"(ctx->Reg0t15[3]),
          "=m"(ctx->Reg0t15[4]),  "=m"(ctx->Reg0t15[5]),
          "=m"(ctx->Reg0t15[6]),  "=m"(ctx->Reg0t15[7]),
          "=m"(ctx->Reg0t15[8]),  "=m"(ctx->Reg0t15[9]),
          "=m"(ctx->Reg0t15[10]), "=m"(ctx->Reg0t15[11]),
          "=m"(ctx->Reg0t15[12]), "=m"(ctx->Reg0t15[13]),
          "=m"(ctx->Reg0t15[14]), "=m"(ctx->Reg0t15[15]),
          "=m"(ctx->cs),  "=m"(ctx->ds),
          "=m"(ctx->es),  "=m"(ctx->fs),
          "=m"(ctx->gs),  "=m"(ctx->ss),
          "=m"(ctx->rflags),
          "=m"(ctx->SIMDBuffer)
        :
        : "memory"
    );
}

static inline void LoadContext(RegContext *ctx) {
    asm volatile(
        "xrstor %22\n"
        "mov %17, %%ds\n"
        "mov %18, %%es\n"
        "mov %19, %%fs\n"
        "mov %20, %%gs\n"
        "mov %21, %%ss\n"
        "pushq %21\n"
        "popfq\n"
        "mov %0,  %%rax\n"
        "mov %1,  %%rcx\n"
        "mov %2,  %%rdx\n"
        "mov %3,  %%rbx\n"
        "mov %5,  %%rbp\n"
        "mov %6,  %%rsi\n"
        "mov %7,  %%rdi\n"
        "mov %8,  %%r8\n"
        "mov %9,  %%r9\n"
        "mov %10, %%r10\n"
        "mov %11, %%r11\n"
        "mov %12, %%r12\n"
        "mov %13, %%r13\n"
        "mov %14, %%r14\n"
        "mov %15, %%r15\n"
        "mov %4,  %%rsp\n"
        :
        : "m"(ctx->Reg0t15[0]),  "m"(ctx->Reg0t15[1]),
          "m"(ctx->Reg0t15[2]),  "m"(ctx->Reg0t15[3]),
          "m"(ctx->Reg0t15[4]),  "m"(ctx->Reg0t15[5]),
          "m"(ctx->Reg0t15[6]),  "m"(ctx->Reg0t15[7]),
          "m"(ctx->Reg0t15[8]),  "m"(ctx->Reg0t15[9]),
          "m"(ctx->Reg0t15[10]), "m"(ctx->Reg0t15[11]),
          "m"(ctx->Reg0t15[12]), "m"(ctx->Reg0t15[13]),
          "m"(ctx->Reg0t15[14]), "m"(ctx->Reg0t15[15]),
          "m"(ctx->ds),  "m"(ctx->es),
          "m"(ctx->fs),  "m"(ctx->gs),
          "m"(ctx->ss),  "m"(ctx->rflags),
          "m"(ctx->SIMDBuffer)
        : "memory"
    );
}

