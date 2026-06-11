#include "debug.h"
#include "str.h"

void DebugStr(const char *S){
    while (*S) DebugChar(*S++);
}

void DebugU64(u64 value) {
    char hex[] = "0000000000000000";
    for (int i = 15; i >= 0; i--) {
        int nibble = (value >> (i * 4)) & 0xF;
        hex[15 - i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    DebugStr(hex);
}

void DebugU64Bit(u64 LLU){
    char buf[65] = "0";
    MemSet((u8*)buf, '0', 64);
    Bit64Str(LLU, buf, 64);
    buf[64]='\0';
    DebugStr(buf);
}

