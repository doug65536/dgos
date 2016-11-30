#pragma once
#include "types.h"

int apic_init(int ap);
unsigned apic_get_id(void);
void apic_send_ipi(int target_apic_id, uint8_t intr);
