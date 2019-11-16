#pragma once
#include "types.h"

template<ptrdiff_t ofs = 0>
static _always_inline void cpu_gs_inc()
{
    __asm__ __volatile__ (
        "incl %%gs:%c[ofs]\n\t"
        :
        : [ofs] "i" (ofs)
        : "cc"
    );
}

template<typename T, ptrdiff_t ofs = 0>
static _always_inline T cpu_gs_read(void)
{
    T value;
    __asm__ __volatile__ (
        "mov %%gs:%c[ofs],%[value]\n\t"
        : [value] "=r" (value)
        : [ofs] "i" (ofs)
        : "memory"
    );
    return value;
}

template<typename T, ptrdiff_t ofs = 0>
static _always_inline T *cpu_gs_ptr(void)
{
    T *value;
    __asm__ __volatile__ (
        "mov %%gs:0,%[value]\n\t"
        : [value] "=r" (value)
        : [ofs] "i" (ofs)
        : "memory"
    );
    // Do the addition outside asm to allow constant folding optimization
    return (T*)((char*)value + ofs);
}
