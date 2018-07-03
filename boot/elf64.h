#pragma once

#include "types.h"

_noreturn
void elf64_run(const tchar *filename);
_pure tchar const *cpu_choose_kernel();
