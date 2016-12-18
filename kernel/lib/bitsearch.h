#pragma once
#include "types.h"

// Return bit number of least significant set bit
static inline int bit_lsb_set_32(int32_t n)
{
    return __builtin_ctz(n);
}

// Return bit number of least significant set bit
static inline int bit_lsb_set_64(int64_t n)
{
    return __builtin_ctzl(n);
}

// Return bit number of most significant set bit
static inline int bit_msb_set_32(int32_t n)
{
    return 32 - __builtin_clz(n);
}

// Return bit number of most significant set bit
static inline int bit_msb_set_64(int64_t n)
{
    return 32 - __builtin_clzl(n);
}

