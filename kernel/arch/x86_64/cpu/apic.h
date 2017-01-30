#pragma once
#include "types.h"

int apic_init(int ap);
void apic_start_smp(void);
unsigned apic_get_id(void);
void apic_send_ipi(int target_apic_id, uint8_t intr);
void apic_eoi(int intr);
uint32_t apic_timer_count(void);
void apic_dump_regs(int ap);

int apic_enable(void);
