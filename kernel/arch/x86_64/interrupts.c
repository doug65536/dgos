#include "interrupts.h"

void interrupts_disable(void)
{
    __asm__ __volatile__ ( "cli" );
}

void interrupts_enable(void)
{
    __asm__ __volatile__ ( "sti" );
}
