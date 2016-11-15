#include "msr.h"

uint64_t msr_get(uint32_t msr)
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

uint32_t msr_get_lo(uint32_t msr)
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

uint32_t msr_get_hi(uint32_t msr)
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

void msr_set(uint32_t msr, uint64_t value)
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

void msr_set_lo(uint32_t msr, uint32_t value)
{
    __asm__ __volatile__ (
        "rdmsr\n\t"
        "mov %[value],%%eax\n\t"
        "wrmsr"
        :
        : [value] "S" (value),
        "c" (msr)
        : "rdx"
    );
}

void msr_set_hi(uint32_t msr, uint32_t value)
{
    __asm__ __volatile__ (
        "rdmsr\n\t"
        "mov %[value],%%edx\n\t"
        "wrmsr"
        :
        : [value] "S" (value),
        "c" (msr)
        : "rdx"
    );
}

uint64_t cpu_cr0_change_bits(uint64_t clear, uint64_t set)
{
    uint64_t rax;
    __asm__ __volatile__ (
        "mov %%cr0,%[result]\n\t"
        "and %[clear],%[result]\n\t"
        "or %[set],%[result]\n\t"
        "mov %[result],%%cr0\n\t"
        : [result] "=&A" (rax)
        : [clear] "ri" (~clear),
        [set] "ri" (set)
    );
    return rax;
}

uint64_t cpu_cr4_change_bits(uint64_t clear, uint64_t set)
{
    uint64_t rax;
    __asm__ __volatile__ (
        "mov %%cr4,%[result]\n\t"
        "and %[clear],%[result]\n\t"
        "or %[set],%[result]\n\t"
        "mov %[result],%%cr4\n\t"
        : [result] "=&A" (rax)
        : [clear] "ri" (~clear),
        [set] "ri" (set)
    );
    return rax;
}

uint64_t cpu_get_fault_address(void)
{
    uint64_t addr;
    __asm__ __volatile__ (
        "mov %%cr2,%0\n\t"
        : "=r" (addr)
    );
    return addr;
}
