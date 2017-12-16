#pragma once
#include "types.h"
#include "irq.h"

int apic_init(int ap);
void apic_start_smp(void);
unsigned apic_get_id(void);

// If intr == 2, sends an NMI
// if target_apic_id is <= -2, sends to all CPUs
// if target_apic_id is == -1, sends to other CPUs
// if target_apid_id is >= 0, sends to specific APIC ID
void apic_send_ipi(int target_apic_id, uint8_t intr);

void apic_eoi(int intr);
uint32_t apic_timer_count(void);
void apic_dump_regs(int ap);

int apic_enable(void);
bool ioapic_irq_cpu(int irq, int cpu);

int apic_msi_irq_alloc(msi_irq_mem_t *results, int count,
                       int cpu, bool distribute, intr_handler_t handler);

int acpi_have8259pic(void);

extern "C" isr_context_t *apic_dispatcher(int intr, isr_context_t *ctx);
