#include "halt.h"
#include "cpu/control_regs.h"
#include "thread.h"

void halt_forever(void)
{
    cpu_irq_disable();

    while (1)
        halt();
}

