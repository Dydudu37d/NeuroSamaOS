#pragma once

#include "int.h"
#include "str.h"

static inline void outb(u16 port,u8 data){
    __asm__ volatile("outb %0, %1" : : "a"(data), "d"(port));
}

static inline u8 inb(u16 port){
    u8 data;
    __asm__ volatile("inb %1, %0" : "=a"(data) : "d"(port));
    return data;
}

static inline void outw(u16 port,u16 data){
    __asm__ volatile("outw %0, %1" : : "a"(data), "d"(port));
}

static inline u16 inw(u16 port){
    u16 data;
    __asm__ volatile("inw %1, %0" : "=a"(data) : "d"(port));
    return data;
}

static inline void outl(u16 port,u32 data){
    __asm__ volatile("outl %0, %1" : : "a"(data), "d"(port));
}

static inline u32 inl(u16 port){
    u32 data;
    __asm__ volatile("inl %1, %0" : "=a"(data) : "d"(port));
    return data;
}

 
