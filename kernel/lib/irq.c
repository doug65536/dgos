#include "irq.h"

void (*irq_handlers[16])(int);

void irq_hook(int irq, void (*handler)(int))
{
    irq_handlers[irq] = handler;
}

void irq_unhook(int irq, void (*handler)(int))
{
    if (irq_handlers[irq] == handler)
        irq_handlers[irq] = 0;
}

void irq_invoke(int irq)
{
    irq_handlers[irq](irq);
}
