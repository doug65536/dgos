#pragma once

extern void (*irq_setmask)(int irq, int unmask);

void irq_hook(int irq, void *(*handler)(int, void*));
void irq_unhook(int irq, void *(*handler)(int, void*));

void *irq_invoke(int irq, void *ctx);
