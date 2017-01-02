#include "irq.h"

// Interrupt handler vectors
static intr_handler_t intr_handlers[128];

static void (*irq_setmask_vec)(int irq, int unmask);
static void (*irq_hook_vec)(int irq, intr_handler_t handler);
static void (*irq_unhook_vec)(int irq, intr_handler_t handler);

void irq_setmask_set_handler(irq_setmask_handler_t handler)
{
    irq_setmask_vec = handler;
}

void irq_hook_set_handler(irq_hook_handler_t handler)
{
    irq_hook_vec = handler;
}

void irq_unhook_set_handler(irq_unhook_handler_t handler)
{
    irq_unhook_vec = handler;
}

void irq_setmask(int irq, int unmask)
{
    irq_setmask_vec(irq, unmask);
}

void irq_hook(int irq, intr_handler_t handler)
{
    irq_hook_vec(irq, handler);
}

void irq_unhook(int irq, intr_handler_t handler)
{
    irq_unhook_vec(irq, handler);
}

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

int intr_has_handler(int intr)
{
    return intr_handlers[intr] != 0;
}

void *irq_invoke(int intr, int irq, void *ctx)
{
    if (intr_handlers[intr])
        return intr_handlers[intr](irq, ctx);
    return ctx;
}
