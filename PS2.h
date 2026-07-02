#pragma once

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_REG   0x64
#define PS2_CMD_REG      0x64

#include "port.h"
#include "int.h"
#include "clock.h"

static inline void PS2WaitWrite() {
    while (inb(PS2_STATUS_REG) & 2);
}

static inline void PS2WaitRead() {
    while (!(inb(PS2_STATUS_REG) & 1));
}

static inline void PS2WriteCmd(u8 cmd) {
    PS2WaitWrite();
    outb(PS2_CMD_REG, cmd);
}

static inline u8 PS2ReadData(){
    PS2WaitRead();
    return inb(PS2_DATA_PORT);
}

static inline u8 PS2ReadDataWithStatus(u8* status){
    PS2WaitRead();
    *status = inb(PS2_STATUS_REG);
    return inb(PS2_DATA_PORT);
}

static inline _Bool PS2IsMouseData(){
    return inb(PS2_STATUS_REG) & 0x20;
}

static inline _Bool PS2HasData(){
    return inb(PS2_STATUS_REG) & 0x01;
}

static inline _Bool PS2ReadDataTimeout(u8 *out, u64 timeoutMs){
    u64 start = SystemGetTimeMillis();
    while (SystemGetTimeMillis() - start < timeoutMs){
        if (PS2HasData()){
            *out = inb(PS2_DATA_PORT);
            return 1;
        }
    }
    return 0;
}