#pragma once

#include "int.h"
#include "efi.h"

static inline u64 rdtsc(void) {
    u32 low, high;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    return ((u64)high << 32) | low;
}

static inline u64 rdtscp(void) {
    u32 low, high, aux;
    __asm__ __volatile__("rdtscp" : "=a"(low), "=d"(high), "=c"(aux));
    return ((u64)high << 32) | low;
}

static inline u64 rdmsr(u32 msr) {
    u32 low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((u64)high << 32) | low;
}

static inline void wrmsr(u32 msr, u64 value) {
    u32 low = value & 0xFFFFFFFF;
    u32 high = value >> 32;
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static inline u64 rdtsc_serialized(void) {
    u32 low, high;
    __asm__ __volatile__(
        "lfence\n\t"
        "rdtsc\n\t"
        : "=a"(low), "=d"(high)
        :
        : "%rbx", "%rcx", "memory"
    );
    return ((u64)high << 32) | low;
}

static inline u64 CalibrateTscUsingStall(void) {
    extern EFI_BOOT_SERVICES* bs;
    u64 StartTsc, EndTsc;

    StartTsc = rdtsc_serialized();
    bs->Stall(100000);
    EndTsc = rdtsc_serialized();

    return (EndTsc - StartTsc) * 10;
}

void InitClock(void);

u64 SystemGetTime(void);
u64 SystemGetTimeNano(void);
u64 SystemGetTimeMillis(void);
u64 GetTscFrequency(void);
void SetTscFrequency(u64 f);

void SystemBusySleepS(u64 Time);
void SystemBusySleepNano(u64 Time);
void SystemBusySleepMs(u64 Time);

