#pragma once

#include "types.h"

#ifdef __clang__
static _always_inline void __builtin_ia32_movntdq(
        __i64_vec2LL* d, __i64_vec2LL const& val128)
{
    __asm__ __volatile__ (
        "movntdq %[src],%[dst]"
        :
        : [dst] "m" (*d)
        , [src] "x" ((__i64_vec2LL)val128)
    );
}
#endif
