#pragma once
#include "types.h"

extern "C" void cpu_init_early(int ap);
extern "C" void cpu_init(int ap);

extern "C" void cpu_init_stage2(int ap);
extern "C" void cpu_hw_init(int ap);

extern "C" void cpu_patch_code(void *addr, void const *src, size_t size);
extern "C" void cpu_patch_insn(void *addr, uint64_t value, size_t size);
extern "C" void cpu_patch_nop(void *addr, size_t size);

extern "C"
void cpu_patch_calls(void *call_target, size_t point_count, uint32_t **points);

extern "C" uint32_t default_mxcsr_mask;

bool cpu_msr_set_safe(uint32_t msr, uint32_t value);
bool cpu_msr_get_safe(uint32_t msr, uint64_t &value);
void cpu_init_late_msrs();
