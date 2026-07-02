#include "PS2Keyboard.h"
#include "PS2.h"
#include "int.h"
#include "debug.h"

_Bool ShiftDown=0;
_Bool CapsLockOpen=0;

char KeyCodeSet1[] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
    0, '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 0,
    0, 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+',
    '1', '2', '3', '0', '.'
};

char KeyCodeSet2[] = {
    '1', '2', 0, 'Q', 'A', 'Z', 0, 'S', 'X',
    0, 'D', 'C', 'F', 'V', 'T', 'G', 'B', 0, ' ', 'E', 'R', '4', '3', 0,
    0, 'Y', 'H', 'N', 'U', 'J', 'M', 0, 0, 'I', 'K', ',', '.', '6', '5', 0,
    0, 'O', 'L', ';', 'P', '/', 0, 0, '\'', 0, '[', '8', '7', 0,
    0, 0, ']', 0, '\\', 0, '\n', '-', 0, 0, 0, '9', '0', 0,
    0, 0, 0, 0, 0, 0, '\b', 0, 0, '1', 0, '4', '7', 0, 0, 0,
    '0', '.', '5', '6', '8', 0, 0, 0, 0, '3', '-', '2', '+'
};

void PS2KeyboardSetLED(u8 Which){
    if (Which>3) return;
    PS2WaitWrite();
    outb(PS2_DATA_PORT, 0xED);
    PS2WaitWrite();
    outb(PS2_DATA_PORT, Which);
}

void *PS2KeyboardPoll(void *Arg) {
    if (!Arg) return NULL;
    if (!PS2HasData()) return Arg;

    u8 Got = PS2ReadData();
    u8 status = inb(PS2_STATUS_REG);
    if (status & 0x20) {
        inb(PS2_DATA_PORT);
        *(char*)Arg = 0;
        return Arg;
    }
    
    if (Got==0x2A || Got==0x36){
        ShiftDown=1;
        *(char*)Arg = 0;
        return Arg;
    }
    if (Got==0xAA || Got==0xB6){
        ShiftDown=0;
        *(char*)Arg = 0;
        return Arg;
    }
    if (Got==0xBA){
        CapsLockOpen=!CapsLockOpen;
        *(char*)Arg = 0;
        return Arg;
    }
    if (Got>=0x80) {
        *(char*)Arg = 0;
        return Arg;
    }
    if (Got>=0x02 && Got<=0x53) {
        char c = KeyCodeSet1[Got - 0x02];
        if (!c) {
            *(char*)Arg = 0;
            return Arg;
        }
        
        if (c>='A' && c<='Z') {
            if (!ShiftDown && !CapsLockOpen) {
                c += 32;
            }
            if (ShiftDown && CapsLockOpen) {
                c += 32;
            }
        }
        DebugChar(c);
        *(char*)Arg = c;
        return Arg;
    }
    *(char*)Arg = 0;
    return Arg;
}

void PS2KeyboardInit(){
    while (PS2HasData()) {
        inb(PS2_DATA_PORT);
    }

    PS2WaitWrite();
    outb(PS2_CMD_REG, 0xAD);
    
    PS2WaitWrite();
    outb(PS2_DATA_PORT, 0xFF);

    u8 ack = 0;
    if (!PS2ReadDataTimeout(&ack, 10)) return;
    if (ack == 0xFA) {
        PS2ReadDataTimeout(&ack, 10);
    }

    PS2WaitWrite();
    outb(PS2_DATA_PORT, 0xF0);
    PS2WaitWrite();
    outb(PS2_DATA_PORT, 0x01);
    PS2ReadDataTimeout(&ack, 10);
    
    PS2WaitWrite();
    outb(PS2_DATA_PORT, 0xF4);
    PS2ReadDataTimeout(&ack, 10);
    
    PS2WaitWrite();
    outb(PS2_CMD_REG, 0xAE);
}