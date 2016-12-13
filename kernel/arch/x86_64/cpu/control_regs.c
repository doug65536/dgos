#include "control_regs.h"

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
        "movq %%cr0,%[result]\n\t"
        "andq %[clear],%[result]\n\t"
        "orq %[set],%[result]\n\t"
        "movq %[result],%%cr0\n\t"
        : [result] "=&r" (rax)
        : [clear] "r" (~clear),
          [set] "r" (set)
    );
    return rax;
}

uint64_t cpu_cr4_change_bits(uint64_t clear, uint64_t set)
{
    uint64_t rax;
    __asm__ __volatile__ (
        "movq %%cr4,%[result]\n\t"
        "andq %[clear],%[result]\n\t"
        "orq %[set],%[result]\n\t"
        "movq %[result],%%cr4\n\t"
        : [result] "=&r" (rax)
        : [clear] "r" (~clear),
          [set] "r" (set)
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

void cpu_set_page_directory(uint64_t addr)
{
    __asm__ __volatile__ (
        "mov %0,%%cr3\n\t"
        :
        : "r" (addr)
        : "memory"
    );
}

uint64_t cpu_get_page_directory(void)
{
    uint64_t addr;
    __asm__ __volatile__ (
        "mov %%cr3,%0\n\t"
        : "=r" (addr)
    );
    return addr;
}

void cpu_set_fsbase(void *fs_base)
{
    msr_set(MSR_FSBASE, (uint64_t)fs_base);
}

void cpu_set_gsbase(void *gs_base)
{
    msr_set(MSR_GSBASE, (uint64_t)gs_base);
}

void cpu_invalidate_page(uint64_t addr)
{
    __asm__ __volatile__ (
        "invlpg (%0)\n\t"
        :
        : "r" (addr)
        : "memory"
    );
}

void cpu_flush_tlb(void)
{
    cpu_set_page_directory(cpu_get_page_directory() & -4096L);
}

table_register_64_t cpu_get_gdtr(void)
{
    table_register_64_t gdtr;
    __asm__ __volatile__ (
        "sgdtq %0\n\t"
        : "=m" (gdtr)
        :
        : "memory"
    );
    return gdtr;
}

void cpu_set_gdtr(table_register_64_t gdtr)
{
    __asm__ __volatile__ (
        "lgdtq (%0)\n\t"
        :
        : "r" (&gdtr)
        : "memory"
    );
}

uint16_t cpu_get_tr(void)
{
    uint16_t tr;
    __asm__ __volatile__ (
        "str %0\n\t"
        : "=m" (tr)
        :
        : "memory"
    );
    return tr;
}

void cpu_set_tr(uint16_t tr)
{
    __asm__ __volatile__ (
        "ltr %0\n\t"
        :
        : "m" (tr)
        : "memory"
    );
}

int cpu_irq_disable(void)
{
    uint64_t rflags;
    __asm__ __volatile__ (
        "pushf\n\t"
        "pop %[rflags]\n\t"
        "cli\n\t"
        : [rflags] "=r" (rflags)
    );
    return !!((rflags >> 9) & 1);
}

void cpu_irq_enable(void)
{
    __asm__ __volatile__ ( "sti" );
}

uint64_t msr_adj_bit(uint32_t msr, int bit, int set)
{
    uint64_t n = msr_get(msr);
    n &= ~((uint64_t)1 << bit);
    n |= (uint64_t)!!set << bit;
    msr_set(msr, n);
    return n;
}
