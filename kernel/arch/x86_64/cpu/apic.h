#pragma once
#include "types.h"
#include "irq.h"

int apic_init(int ap);
void apic_init_timer();
void apic_start_smp(void);
void apic_init_apic_smp();
unsigned apic_get_id(void);
size_t apic_cpu_count();

// If intr == 2, sends an NMI
// if target_apic_id is <= -2, sends to all CPUs
// if target_apic_id is == -1, sends to other CPUs
// if target_apid_id is >= 0, sends to specific APIC ID
void apic_send_ipi(int32_t target_apic_id, uint8_t intr);
void apic_send_ipi_noinst(int32_t target_apic_id, uint8_t intr);

void apic_eoi(int intr);
void apic_eoi_noinst(int intr);
uint32_t apic_timer_count(void);
void apic_dump_regs(int ap);

KERNEL_API uint64_t apic_configure_timer(uint64_t ticks, bool one_shot, bool mask);
KERNEL_API uint64_t apic_timer_hw_oneshot(uint8_t &dcr_shadow, uint64_t icr);

bool apic_enable(void);
bool ioapic_irq_setcpu(int irq, int cpu);

bool apic_request_pending(int intr);
bool apic_request_in_service(int intr);
bool apic_request_is_level(int intr);

void apic_msi_target(msi_irq_mem_t *result, int cpu, int vector);

int apic_msi_irq_alloc(msi_irq_mem_t *results, int count,
                       int cpu, bool distribute, intr_handler_t handler,
                       char const *name, int const *target_cpus = nullptr,
                       int const *vector_offsets = nullptr,
                       bool aligned = false);

void apic_config_cpu();

void apic_hook_perf_local_irq(intr_handler_t handler, char const *name,
                              bool direct);

int acpi_have8259pic(void);

struct memory_affinity_t {
    uint64_t base;
    uint64_t length;
    uint32_t domain;
};



_pure
uint32_t acpi_cpu_count();

uint64_t apic_ns_to_ticks(uint64_t ns);

extern "C" isr_context_t *apic_dispatcher(int intr, isr_context_t *ctx);
