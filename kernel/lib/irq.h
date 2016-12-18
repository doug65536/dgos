#pragma once

typedef void *(*intr_handler_t)(int intr, void*);

//
// Vectors set by interrupt controller implementation

// Change irq mask
extern void (*irq_setmask)(int irq, int unmask);

// Set the appropriate interrupt vector for the specified irq
extern void (*irq_hook)(int irq, intr_handler_t handler);

// Reset the appropriate interrupt vector for the specified irq
extern void (*irq_unhook)(int irq, intr_handler_t handler);

//
// Interrupt vector manipulation and dispatch

// Set interrupt vector
void intr_hook(int intr, intr_handler_t handler);

// Reset interrupt vector
void intr_unhook(int intr, intr_handler_t handler);

// Call the interrupt handler
void *intr_invoke(int intr, void *ctx);

// Call the appropriate interrupt vector for the specified irq
void *irq_invoke(int intr, int irq, void *ctx);

// Returns true if there is an interrupt handler for the interrupt
int intr_has_handler(int intr);
