#pragma once
#include "types.h"

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
        "add %[ofs],%[value]\n\t"
        : [value] "=r" (value)
        : [ofs] "i" (ofs)
        : "memory"
    );
    return value;
}
