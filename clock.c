#include "clock.h"

static u64 g_tsc_frequency = 0;
static u64 g_tsc_boot_offset = 0;

void InitClock(void) {
    if (g_tsc_frequency == 0) {
        g_tsc_frequency = CalibrateTscUsingStall();
    }
    g_tsc_boot_offset = rdtsc_serialized();
}

u64 SystemGetTime(void) {
    u64 tsc = rdtsc_serialized() - g_tsc_boot_offset;
    return tsc * 1000000 / g_tsc_frequency;
}

u64 SystemGetTimeNano(void) {
    u64 tsc = rdtsc_serialized() - g_tsc_boot_offset;
    return tsc*1000000000/g_tsc_frequency;
}

u64 SystemGetTimeMillis(void) {
    u64 tsc = rdtsc_serialized() - g_tsc_boot_offset;
    return tsc * 1000 / g_tsc_frequency;
}

u64 GetTscFrequency(void) {
    return g_tsc_frequency;
}

void SetTscFrequency(u64 f){
    g_tsc_frequency=f;
}

void SystemBusySleepS(u64 Time){
    u64 NowTime=SystemGetTime();
    while (SystemGetTime() > NowTime+Time);
}

void SystemBusySleepNano(u64 Time){
    u64 NowTime=SystemGetTimeNano();
    while (SystemGetTimeNano() > NowTime+Time);
}

void SystemBusySleepMs(u64 Time){
    u64 NowTime=SystemGetTimeMillis();
    while (SystemGetTimeMillis() > NowTime+Time);
}

