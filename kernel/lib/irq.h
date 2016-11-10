#pragma once

void irq_hook(int irq, void (*handler)(int));
void irq_unhook(int irq, void (*handler)(int));

void irq_invoke(int irq);
