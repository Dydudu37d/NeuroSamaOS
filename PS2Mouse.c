#include "PS2Mouse.h"
#include "PS2.h"
#include "port.h"
#include "int.h"

void PS2MouseWrite(u8 data){
    PS2WaitWrite(); outb(PS2_CMD_REG, 0xD4);
    PS2WaitWrite(); outb(PS2_DATA_PORT, data);
}

u8 PS2MouseRead(){
    PS2WaitRead();
    return inb(PS2_DATA_PORT);
}

MouseState PS2GetMouseState(){
    MouseState state = {0};
    
    PS2WaitRead();
    u8 b0 = inb(PS2_DATA_PORT);
    
    PS2WaitRead();
    u8 b1 = inb(PS2_DATA_PORT);
    
    PS2WaitRead();
    u8 b2 = inb(PS2_DATA_PORT);

    state.LButton = b0 & 0x01;
    state.RButton = (b0 >> 1) & 0x01;
    state.MButton = (b0 >> 2) & 0x01;
    state.x = b1;
    if (b0 & 0x10) {
        state.x -= 256;
    }
    state.y = b2;
    if (b0 & 0x20) {
        state.y -= 256;
    }

    return state;
}

void PS2MouseInit(){
    PS2WaitWrite();
    outb(PS2_CMD_REG, 0xA8); 
    PS2MouseWrite(0xF4);
    PS2MouseRead();
}

void PS2MouseSetDPI(u8 DPI) {
    PS2MouseWrite(0xE8);
    PS2MouseRead(); 
    PS2MouseWrite(DPI);
    PS2MouseRead(); 
}