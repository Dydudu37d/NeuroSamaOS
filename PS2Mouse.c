#include "PS2Mouse.h"
#include "PS2.h"
#include "port.h"
#include "int.h"

void PS2MouseWrite(u8 data){
    PS2WaitWrite(); 
    outb(PS2_CMD_REG, 0xD4);
    PS2WaitWrite(); 
    outb(PS2_DATA_PORT, data);
}

u8 PS2MouseRead(){
    PS2WaitRead();
    return inb(PS2_DATA_PORT);
}

void *PS2GetMouseState(void *Arg){
    if (!Arg) return NULL;

    MouseState *state = (MouseState*)Arg;
    *state = (MouseState){0};

    u8 status = inb(PS2_STATUS_REG);
    if (!(status & 0x20)) {
        return Arg;
    }
    
    u8 b0 = PS2ReadData();
    u8 b1 = PS2ReadData();
    u8 b2 = PS2ReadData();

    state->LButton = b0 & 0x01;
    state->RButton = (b0 >> 1) & 0x01;
    state->MButton = (b0 >> 2) & 0x01;
    
    state->x = b1;
    if (b0 & 0x10) {
        state->x |= 0xFF00;
    }
    
    state->y = b2;
    if (b0 & 0x20) {
        state->y |= 0xFF00;
    }

    return Arg;
}

void PS2MouseInit(){
    PS2WaitWrite();
    outb(PS2_CMD_REG, 0xA8);
    PS2MouseWrite(0xF4);
    PS2MouseRead();
}

void PS2MouseSetDPI(u8 DPI) {
    if (DPI > 3) DPI = 3;
    PS2MouseWrite(0xE8);
    PS2MouseRead();
    PS2MouseWrite(DPI);
    PS2MouseRead();
}