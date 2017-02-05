#pragma once
#include "types.h"
#include "irq.h"

int apic_init(int ap);
void apic_start_smp(void);
unsigned apic_get_id(void);
void apic_send_ipi(int target_apic_id, uint8_t intr);
void apic_eoi(int intr);
uint32_t apic_timer_count(void);
void apic_dump_regs(int ap);

int apic_enable(void);
int ioapic_irq_cpu(int irq, int cpu);

int apic_msi_irq_alloc(msi_irq_mem_t *results, int count,
                       int cpu, int distribute, intr_handler_t handler);
