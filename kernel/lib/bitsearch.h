#pragma once
#include "types.h"

// Return bit number of least significant set bit
static inline uint8_t bit_lsb_set_32(int32_t n)
{
    return __builtin_ctz(n);
}

// Return bit number of least significant set bit
static inline uint8_t bit_lsb_set_64(int64_t n)
{
    return __builtin_ctzl(n);
}

// Return bit number of most significant set bit
static inline uint8_t bit_msb_set_32(int32_t n)
{
    return 31 - __builtin_clz(n);
}

// Return bit number of most significant set bit
static inline uint8_t bit_msb_set_64(int64_t n)
{
    return 63 - __builtin_clzl(n);
}

// return ceil(log(n) / log(2))
static inline uint8_t bit_log2_n_32(int32_t n)
{
    int top = bit_msb_set_32(n);
    return top + !!(~(-1 << top) & n);
}

// return ceil(log(n) / log(2))
static inline uint8_t bit_log2_n_64(int64_t n)
{
    int top = bit_msb_set_64(n);
    return top + !!(~((int64_t)-1 << top) & n);
}

