#pragma once

#include "types.h"

typedef struct cpuid_t {
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
} cpuid_t;

// Force use of CPUID instruction (for getting APIC ID)
// Returns true if the CPU supports that leaf
int cpuid_nocache(cpuid_t *output, uint32_t eax, uint32_t ecx);

// Allow cached data
// Returns true if the CPU supports that leaf
int cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx);

int cpuid_eax_bit(int bit, uint32_t eax, uint32_t ecx);
int cpuid_ebx_bit(int bit, uint32_t eax, uint32_t ecx);
int cpuid_ecx_bit(int bit, uint32_t eax, uint32_t ecx);
int cpuid_edx_bit(int bit, uint32_t eax, uint32_t ecx);

#define CPUID_INFO_FEATURES     1

static inline int cpuid_has_sse3(void)   { return cpuid_ecx_bit(0, 1, 0); }
static inline int cpuid_has_mwait(void)  { return cpuid_ecx_bit(3, 1, 0); }
static inline int cpuid_has_ssse3(void)  { return cpuid_ecx_bit(9, 1, 0); }
static inline int cpuid_has_fma(void)    { return cpuid_ecx_bit(12, 1, 0); }
static inline int cpuid_has_pcid(void)   { return cpuid_ecx_bit(17, 1, 0); }
static inline int cpuid_has_sse4_1(void) { return cpuid_ecx_bit(19, 1, 0); }
static inline int cpuid_has_sse4_2(void) { return cpuid_ecx_bit(20, 1, 0); }
static inline int cpuid_has_x2apic(void) { return cpuid_ecx_bit(21, 1, 0); }
static inline int cpuid_has_aes(void)    { return cpuid_ecx_bit(25, 1, 0); }
static inline int cpuid_has_xsave(void)  { return cpuid_ecx_bit(26, 1, 0); }
static inline int cpuid_has_avx(void)    { return cpuid_ecx_bit(28, 1, 0); }
static inline int cpuid_has_rdrand(void) { return cpuid_ecx_bit(30, 1, 0); }
