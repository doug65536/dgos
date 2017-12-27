#pragma once
#include "types.h"

extern "C" void cpu_init(int ap);
extern "C" void cpu_init_stage2(int ap);
extern "C" void cpu_hw_init(int ap);

extern "C" void cpu_patch_code(void *addr, void const *src, size_t size);
extern "C" void cpu_patch_insn(void *addr, uint64_t value, size_t size);
