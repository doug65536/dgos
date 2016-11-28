#pragma once

void cpu_init(int ap);
void cpu_hw_init(void);

int irq_hook(int irq, void (*handler)(void));
int irq_unhook(int irq, void (*handler)(void));

int irq_is_pending(int irq);
int irq_ack(int irq);
