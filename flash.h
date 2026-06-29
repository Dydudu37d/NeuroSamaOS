#pragma once

static inline void CompilerBarrier(void) {
    asm volatile("" ::: "memory");
}

static inline void MemFlash(void) {
    asm volatile("sfence" ::: "memory");
}

static inline void LoadFlash(void) {
    asm volatile("lfence" ::: "memory");
}

static inline void MemFullFlash(void) {
    asm volatile("mfence" ::: "memory");
}
