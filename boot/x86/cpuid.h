#pragma once
#include "types.h"

__BEGIN_DECLS

struct cpuid_t {
    uint32_t eax;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
};

bool cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx);
bool cpu_has_long_mode();
bool cpu_has_global_pages();
bool cpu_has_bmi();
bool cpu_has_upto_avx2();

__END_DECLS
