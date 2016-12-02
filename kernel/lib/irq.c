#include "irq.h"

// Interrupt handler vectors
static intr_handler_t intr_handlers[128];

void (*irq_setmask)(int irq, int unmask);
void (*irq_hook)(int irq, intr_handler_t handler);
void (*irq_unhook)(int irq, intr_handler_t handler);

void intr_hook(int intr, intr_handler_t handler)
{
    intr_handlers[intr] = handler;
}

void intr_unhook(int irq, intr_handler_t handler)
{
    if (intr_handlers[irq] == handler)
        intr_handlers[irq] = 0;
}

void *intr_invoke(int intr, void *ctx)
{
    if (intr_handlers[intr])
        return intr_handlers[intr](intr, ctx);
    return ctx;
}

void *irq_invoke(int intr, int irq, void *ctx)
{
    if (intr_handlers[intr])
        return intr_handlers[intr](irq, ctx);
    return ctx;
}
