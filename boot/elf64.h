#pragma once

#include "types.h"

struct elf64_loadaddr_t {
    uint64_t entry;
    uint64_t base;
};

void elf64_boot();

_use_result
elf64_loadaddr_t elf64_load(tchar const *filename);

_pure tchar const *cpu_choose_kernel();

extern "C" _noreturn
void enter_kernel(uint64_t entry_point, uint64_t base) _section(".smp.text");

