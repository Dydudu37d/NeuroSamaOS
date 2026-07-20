#include "clock.h"

static u64 g_tsc_frequency = 0;
static u64 g_tsc_boot_offset = 0;
static u64 g_tsc_to_ms_mult = 0; 
static u64 g_tsc_to_ns_mult = 0;
static u64 g_tsc_to_us_mult = 0;
static u64 g_tsc_to_s_mult = 0;
static u32 g_tsc_shift = 32;

void InitClock(void) {
    if (g_tsc_frequency == 0) {
        g_tsc_frequency = CalibrateTscUsingStall();
    }
    g_tsc_boot_offset = rdtsc_serialized();

    g_tsc_to_s_mult  = (1ULL << g_tsc_shift) / g_tsc_frequency;
    g_tsc_to_ms_mult = (1000ULL << g_tsc_shift) / g_tsc_frequency;
    g_tsc_to_us_mult = (1000000ULL << g_tsc_shift) / g_tsc_frequency;
    g_tsc_to_ns_mult = (1000000000ULL << g_tsc_shift) / g_tsc_frequency;
}

u64 SystemGetTime(void) {
    u64 tsc = rdtsc_serialized() - g_tsc_boot_offset;
    return (tsc * g_tsc_to_us_mult) >> g_tsc_shift;
}

u64 SystemGetTimeMillis(void) {
    u64 tsc = rdtsc_serialized() - g_tsc_boot_offset;
    return (tsc * g_tsc_to_ms_mult) >> g_tsc_shift;
}

u64 SystemGetTimeNano(void) {
    u64 tsc = rdtsc_serialized() - g_tsc_boot_offset;
    return (tsc * g_tsc_to_ns_mult) >> g_tsc_shift;
}

u64 SystemGetTimeS(void) {
    u64 tsc = rdtsc_serialized() - g_tsc_boot_offset;
    return (tsc * g_tsc_to_s_mult) >> g_tsc_shift;
}

u64 GetTscFrequency(void) {
    return g_tsc_frequency;
}

void SetTscFrequency(u64 f){
    g_tsc_frequency=f;
}

void SystemBusySleepS(u64 Time){
    u64 NowTime=SystemGetTimeS();
    while (SystemGetTimeS() < NowTime+Time);
}

void SystemBusySleepUs(u64 Time){
    u64 NowTime=SystemGetTime();
    while (SystemGetTime() < NowTime+Time);
}

void SystemBusySleepNano(u64 Time){
    u64 NowTime=SystemGetTimeNano();
    while (SystemGetTimeNano() < NowTime+Time);
}

void SystemBusySleepMs(u64 Time){
    u64 NowTime=SystemGetTimeMillis();
    while (SystemGetTimeMillis() < NowTime+Time);
}
