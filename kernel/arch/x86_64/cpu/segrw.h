#pragma once
#include "types.h"
#include "string.h"

template<typename T, ptrdiff_t ofs = 0>
static _always_inline T *cpu_gs_ptr(void)
{
    uintptr_t value = 0;

    __asm__ __volatile__ (
        "mov %%gs:(%[value]),%[value]\n\t"
        : [value] "+r" (value)
        :
        : "memory"
    );

    // Do the addition outside asm to allow constant folding optimization
    return reinterpret_cast<T*>(value + ofs);
}

template<ptrdiff_t ofs = 0>
static _always_inline void cpu_gs_inc()
{
    __atomic_fetch_add(cpu_gs_ptr<int, ofs>(), 1, __ATOMIC_RELAXED);
}

template<typename T, ptrdiff_t ofs = 0>
static _always_inline T cpu_gs_read(void)
{
    T value;
    memcpy(&value, cpu_gs_ptr<T, ofs>(), sizeof(T));
    return value;
}
