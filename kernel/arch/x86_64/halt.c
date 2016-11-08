#include "halt.h"

void interrupts_disable(void)
{
    __asm__ __volatile__ ( "cli" );
}

void interrupts_enable(void)
{
    __asm__ __volatile__ ( "sti" );
}

void halt(void)
{
    __asm__ __volatile__ ( "hlt" );
}

void halt_forever(void)
{
    interrupts_disable();

    while (1)
        halt();
}
