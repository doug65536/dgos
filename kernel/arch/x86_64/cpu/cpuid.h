#pragma once

#include "types.h"

typedef struct cpuid_t {
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
} cpuid_t;

// Returns true if the CPU supports that leaf
int cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx);

int cpuid_eax_bit(int bit, uint32_t eax, uint32_t ecx);
int cpuid_ebx_bit(int bit, uint32_t eax, uint32_t ecx);
int cpuid_ecx_bit(int bit, uint32_t eax, uint32_t ecx);
int cpuid_edx_bit(int bit, uint32_t eax, uint32_t ecx);

#define CPUID_INFO_FEATURES     1
