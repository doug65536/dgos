#pragma once
#include "types.h"

__BEGIN_DECLS

void cpu_init_early(int ap);
void cpu_init(int ap);

void cpu_init_stage2(int ap);
void cpu_hw_init(int ap);

void cpu_patch_code(void *addr, void const *src, size_t size);
void cpu_patch_insn(void *addr, uint64_t value, size_t size);
void cpu_patch_nop(void *addr, size_t size);
bool cpu_patch_jmp(void *addr, size_t size, const void *jmp_target);

void cpu_patch_calls(void const *call_target,
                     size_t point_count, uint32_t **points);

extern uint32_t default_mxcsr_mask;

bool cpu_msr_set_safe(uint32_t msr, uint64_t value);
bool cpu_msr_get_safe(uint32_t msr, uint64_t &value);
void cpu_init_late_msrs();
_noreturn
void cpu_init_ap();

bool arch_irq_disable();
void arch_irq_enable();
void arch_irq_toggle(bool en);

__END_DECLS
