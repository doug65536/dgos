#pragma once
#include "types.h"

struct cpuid_t {
    uint32_t eax;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
};

bool cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx);
__pure bool cpu_has_long_mode();
__pure bool cpu_has_no_execute();
__pure bool cpu_has_global_pages();
__pure bool cpu_has_bmi();
