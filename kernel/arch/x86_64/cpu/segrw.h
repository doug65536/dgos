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
        "mov %%gs:(%[ofs]),%[value]\n\t"
        : [value] "=r" (value)
        : [ofs] "r" (ofs)
        : "memory"
    );
    return value;
}

template<typename T, ptrdiff_t ofs = 0>
static _always_inline T *cpu_gs_ptr(void)
{
    uintptr_t value = 0;
    // Probably better to load a register with zero than use
    // huge absolute memory encoding instruction
    __asm__ __volatile__ (
        "mov %%gs:(%[value]),%[value]\n\t"
        : [value] "+r" (value)
        :
        : "memory"
    );
    // Do the addition outside asm to allow constant folding optimization
    return reinterpret_cast<T*>(value + ofs);
}
