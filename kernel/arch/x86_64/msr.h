#pragma once

#include "types.h"

#define MSR_FSBASE      0xC0000100
#define MSR_GSBASE      0xC0000101

// Get whole MSR register as a 64-bit value
extern inline uint64_t msr_get(uint32_t msr)
{
    uint64_t result;
    __asm__ __volatile__ (
        "rdmsr\n\t"
        "shl $32,%%rdx\n\t"
        "or %%rdx,%%rax\n\t"
        : "=a" (result)
        : "c" (msr)
        : "rdx"
    );
    return result;
}

// Get low 32 bits pf MSR register
extern inline uint32_t msr_get_lo(uint32_t msr)
{
    uint64_t result;
    __asm__ __volatile__ (
        "rdmsr\n\t"
        : "=a" (result)
        : "c" (msr)
        : "rdx"
    );
    return result;
}

// High high 32 bits of MSR register
extern inline uint32_t msr_get_hi(uint32_t msr)
{
    uint64_t result;
    __asm__ __volatile__ (
        "rdmsr\n\t"
        : "=d" (result)
        : "c" (msr)
        : "rax"
    );
    return result;
}

// Set whole MSR as a 64 bit value
extern inline void msr_set(uint32_t msr, uint64_t value)
{
    __asm__ __volatile__ (
        "mov %%rax,%%rdx\n\t"
        "shr $32,%%rdx\n\t"
        "wrmsr\n\t"
        :
        : "a" (value),
          "c" (msr)
        : "rdx"
    );
}

// Update the low 32 bits of MSR, preserving the high 32 bits
extern inline void msr_set_lo(uint32_t msr, uint32_t value)
{
    __asm__ __volatile__ (
        "rdmsr\n\t"
        "mov %%esi,%%eax\n\t"
        "wrmsr"
        :
        : "S" (value),
          "c" (msr)
        : "rdx"
    );
}

// Update the low 32 bits of MSR, preserving the high 32 bits
extern inline void msr_set_hi(uint32_t msr, uint32_t value)
{
    __asm__ __volatile__ (
        "rdmsr\n\t"
        "mov %%esi,%%edx\n\t"
        "wrmsr"
        :
        : "S" (value),
          "c" (msr)
        : "rdx"
    );
}
