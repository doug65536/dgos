#pragma once

#include "types.h"

#if defined(__x86_64__) || defined(__i386__)
#include "x86/arch_cpu.h"
#elif defined(__aarch64__)
#include "aarch64/arch_cpu.h"
#else
#error Unknown processor
#endif
