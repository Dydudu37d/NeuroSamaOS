#pragma once

#include "int.h"

typedef struct Obj{
    u64 w,h,x,y;
    _Bool Visible;
    void (*Draw)(void* self);
} Obj;
typedef struct Window Window;
typedef struct Window{
    Obj Base;
    char* Title;
    _Bool Hide;
    void (*Poll)(Window* self);
    u64 Count;
    Obj *Objs[];
} Window;

typedef struct TextBox{
    Obj Base;
    char* Text;
    void (*Poll)(void);
} TextBox;

typedef struct Button{
    Obj Base;
    _Bool Pressed;
    void (*Click)(void);
    void (*Hover)(void);
    void (*Poll)(void);
} Button;
