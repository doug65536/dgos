#pragma once
#include "types.h"

__always_inline
static void halt(void)
{
    __asm__ __volatile__ ( "hlt" );
}

extern "C" __noreturn void halt_forever(void);
