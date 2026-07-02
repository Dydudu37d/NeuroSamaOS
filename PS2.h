#pragma once

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_REG   0x64
#define PS2_CMD_REG      0x64

#include "port.h"
#include "int.h"

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