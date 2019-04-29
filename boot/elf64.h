#pragma once

#include "types.h"

_noreturn
void elf64_run(tchar const *filename);
_pure tchar const *cpu_choose_kernel();
