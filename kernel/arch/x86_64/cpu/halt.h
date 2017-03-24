#pragma once

static inline void halt(void)
{
    __asm__ __volatile__ ( "hlt" );
}

__attribute__((noreturn)) void halt_forever(void);
