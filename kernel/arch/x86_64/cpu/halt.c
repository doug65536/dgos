#include "cpu/halt.h"
#include "control_regs.h"



void halt_forever(void)
{
    cpu_irq_disable();

    while (1)
        halt();
}

