#pragma once

void halt(void);

static inline void pause(void)
{
    __asm__ __volatile__ ( "pause" );
}

__attribute__((noreturn)) void halt_forever(void);
