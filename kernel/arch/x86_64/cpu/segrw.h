#pragma once
#include "types.h"

template<ptrdiff_t ofs = 0>
static __always_inline void *cpu_gs_read_ptr(void)
{
    void *ptr;
    __asm__ __volatile__ (
        "mov %%gs:%c[ofs],%[ptr]\n\t"
        : [ptr] "=r" (ptr)
        : [ofs] "i" (ofs)
        : "memory"
    );
    return ptr;
}

static __always_inline void *cpu_gs_read_ptr(size_t offset)
{
    void *ptr;
    __asm__ __volatile__ (
        "mov %%gs:(%[offset]),%[ptr]\n\t"
        : [ptr] "=r" (ptr)
        : [offset] "r" ((uintptr_t*)offset)
        : "memory"
    );
    return ptr;
}
