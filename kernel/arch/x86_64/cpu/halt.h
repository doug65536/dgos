#pragma once
#include "types.h"

static inline void halt(void)
{
    __asm__ __volatile__ ( "hlt" );
}

extern "C" __noreturn void halt_forever(void);
