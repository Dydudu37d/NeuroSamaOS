#pragma once

static inline void CompilerBarrier(void) {
    __asm__ volatile("" ::: "memory");
}

static inline void MemFlash(void) {
    __asm__ volatile("sfence" ::: "memory");
}

static inline void LoadFlash(void) {
    __asm__ volatile("lfence" ::: "memory");
}

static inline void MemFullFlash(void) {
    __asm__ volatile("mfence" ::: "memory");
}
