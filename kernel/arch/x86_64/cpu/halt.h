#pragma once
#include "types.h"

_always_inline
static void halt(void)
{
    __asm__ __volatile__ ( "hlt" );
}

extern "C" _noreturn void halt_forever(void);
