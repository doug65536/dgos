#pragma once
#include "types.h"

struct cpuid_t {
    uint32_t eax;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
};

extern "C" bool cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx);
extern "C" _pure bool cpu_has_long_mode();
extern "C" _pure bool cpu_has_no_execute();
extern "C" _pure bool cpu_has_global_pages();
extern "C" _pure bool cpu_has_bmi();
