#pragma once

#include "types.h"

#ifdef __clang__
static inline void __builtin_ia32_movntdq(__ivec2LL* d, __ivec2LL const& val128)
{
    __asm__ __volatile__ (
        "vmovntdq %[src],%[dst]"
        :
        : [dst] "m" (*d)
        , [src] "x" ((__ivec2LL)val128)
    );
}
#endif
