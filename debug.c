#include "debug.h"
#include "str.h"

void DebugStr(const char *S){
    while (*S) DebugChar(*S++);
}

void DebugU64Bit(u64 LLU){
    char buf[65] = "0";
    MemSet((u8*)buf, '0', 64);
    Bit64Str(LLU, buf, 64);
    buf[64]='\0';
    DebugStr(buf);
}

void DebugU64(u64 LLU) {
    const char hex[]="0123456789ABCDEF";
    DebugChar(hex[(LLU&0xF000000000000000)>>60]);
    DebugChar(hex[(LLU&0x0F00000000000000)>>56]);
    DebugChar(hex[(LLU&0x00F0000000000000)>>52]);
    DebugChar(hex[(LLU&0x000F000000000000)>>48]);
    DebugChar(hex[(LLU&0x0000F00000000000)>>44]);
    DebugChar(hex[(LLU&0x00000F0000000000)>>40]);
    DebugChar(hex[(LLU&0x000000F000000000)>>36]);
    DebugChar(hex[(LLU&0x0000000F00000000)>>32]);
    DebugChar(hex[(LLU&0x00000000F0000000)>>28]);
    DebugChar(hex[(LLU&0x000000000F000000)>>24]);
    DebugChar(hex[(LLU&0x0000000000F00000)>>20]);
    DebugChar(hex[(LLU&0x00000000000F0000)>>16]);
    DebugChar(hex[(LLU&0x000000000000F000)>>12]);
    DebugChar(hex[(LLU&0x0000000000000F00)>>8 ]);
    DebugChar(hex[(LLU&0x00000000000000F0)>>4 ]);
    DebugChar(hex[(LLU&0x000000000000000F)>>0 ]);
}

void DebugU32(u32 LU) {
    const char hex[]="0123456789ABCDEF";
    DebugChar(hex[(LU&0x00000000F0000000)>>28]);
    DebugChar(hex[(LU&0x000000000F000000)>>24]);
    DebugChar(hex[(LU&0x0000000000F00000)>>20]);
    DebugChar(hex[(LU&0x00000000000F0000)>>16]);
    DebugChar(hex[(LU&0x000000000000F000)>>12]);
    DebugChar(hex[(LU&0x0000000000000F00)>>8 ]);
    DebugChar(hex[(LU&0x00000000000000F0)>>4 ]);
    DebugChar(hex[(LU&0x000000000000000F)>>0 ]);
}

void DebugU16(u16 D) {
    const char hex[]="0123456789ABCDEF";
    DebugChar(hex[(D&0x000000000000F000)>>12]);
    DebugChar(hex[(D&0x0000000000000F00)>>8 ]);
    DebugChar(hex[(D&0x00000000000000F0)>>4 ]);
    DebugChar(hex[(D&0x000000000000000F)>>0 ]);
}

void DebugU8(u8 B) {
    const char hex[]="0123456789ABCDEF";
    DebugChar(hex[(B&0x00000000000000F0)>>4 ]);
    DebugChar(hex[(B&0x000000000000000F)>>0 ]);
}

