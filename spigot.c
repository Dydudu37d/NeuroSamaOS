#include "spigot.h"
#include "int.h"
#include "str.h"
#include "kmalloc.h"

extern AllocPool KernelPool;

void SpigotE(u64 Count, char *OutBuffer) {
    u32* Array = AlignedAlloc(&KernelPool, sizeof(u32) * Count, 64);
    MemSet32(Array, 1, sizeof(u32) * Count);

    u64 BufferIdx = 0;
    
    OutBuffer[BufferIdx++] = '2';
    OutBuffer[BufferIdx++] = '.';

    for (u64 Digit = 0; Digit < Count - 2; Digit++) {
        u32 Carry = 0;

        for (u64 I = Count - 1; I > 0; I--) {
            u32 Base = I + 1;
            u32 Value = Array[I] * 10 + Carry;
            
            Array[I] = Value % Base;
            Carry = Value / Base;
        }

        u32 Value = Array[0] * 10 + Carry;
        Array[0] = Value % 2;
        u32 DigitValue = Value / 2;

        OutBuffer[BufferIdx++] = (char)('0' + DigitValue);
    }

    OutBuffer[BufferIdx] = '\0';
}

void SpigotPi(u64 Count, char *OutBuffer) {
    u32* Array = AlignedAlloc(&KernelPool, sizeof(u32) * Count, 64);
    MemSet32(Array, 2, sizeof(u32) * Count);

    u64 BufferIdx = 0;
    u32 HeldDigit = 0;
    u32 NineCount = 0;
    _Bool FirstDigit = 1;

    for (u64 Iter = 0; Iter < Count; Iter++) {
        u32 Carry = 0;

        for (u64 I = Count - 1; I > 0; I--) {
            u32 Base = 2 * I + 1;
            u32 Value = Array[I] * 10 + Carry;

            Array[I] = Value % Base;
            Carry = (Value / Base) * I;
        }

        u32 Value = Array[0] * 10 + Carry;
        Array[0] = Value % 1;
        u32 DigitValue = Value / 1;

        if (DigitValue == 9) {
            NineCount++;
        } else if (DigitValue > 9) {
            if (FirstDigit) {
                OutBuffer[BufferIdx++] = (char)('0' + HeldDigit + 1);
                OutBuffer[BufferIdx++] = '.';
                FirstDigit = 0;
            } else {
                OutBuffer[BufferIdx++] = (char)('0' + HeldDigit + 1);
            }

            for (u32 N = 0; N < NineCount; N++) {
                OutBuffer[BufferIdx++] = '0';
            }
            HeldDigit = DigitValue - 10;
            NineCount = 0;
        } else {
            if (Iter > 0) {
                if (FirstDigit) {
                    OutBuffer[BufferIdx++] = (char)('0' + HeldDigit);
                    OutBuffer[BufferIdx++] = '.';
                    FirstDigit = 0;
                } else {
                    OutBuffer[BufferIdx++] = (char)('0' + HeldDigit);
                }

                for (u32 N = 0; N < NineCount; N++) {
                    OutBuffer[BufferIdx++] = '9';
                }
            }
            HeldDigit = DigitValue;
            NineCount = 0;
        }
    }

    OutBuffer[BufferIdx++] = (char)('0' + HeldDigit);
    OutBuffer[BufferIdx] = '\0';
}
