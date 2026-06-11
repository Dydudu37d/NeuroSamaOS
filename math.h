#pragma once

#include "int.h"

static inline s64 abs(s64 x){
    return ((x) < 0 ? -(x):(x));
}
