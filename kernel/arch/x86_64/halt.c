#include "halt.h"
#include "interrupts.h"

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

