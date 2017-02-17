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

uintptr_t cpu_cr0_change_bits(uintptr_t clear, uintptr_t set)
{
    uintptr_t rax;
    __asm__ __volatile__ (
        "mov %%cr0,%[result]\n\t"
        "and %[clear],%[result]\n\t"
        "or %[set],%[result]\n\t"
        "mov %[result],%%cr0\n\t"
        : [result] "=&r" (rax)
        : [clear] "r" (~clear),
          [set] "r" (set)
    );
    return rax;
}

uintptr_t cpu_cr4_change_bits(uintptr_t clear, uintptr_t set)
{
    uintptr_t rax;
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

void cpu_set_page_directory(uintptr_t addr)
{
    __asm__ __volatile__ (
        "mov %[addr],%%cr3\n\t"
        :
        : [addr] "r" (addr)
        : "memory"
    );
}

uintptr_t cpu_get_page_directory(void)
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "mov %%cr3,%[addr]\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

void cpu_set_fsbase(void *fs_base)
{
    msr_set(MSR_FSBASE, (uintptr_t)fs_base);
}

void cpu_set_gsbase(void *gs_base)
{
    msr_set(MSR_GSBASE, (uintptr_t)gs_base);
}

void cpu_flush_tlb(void)
{
    cpu_set_page_directory(cpu_get_page_directory() & -4096L);
}

table_register_64_t cpu_get_gdtr(void)
{
    table_register_64_t gdtr;
    __asm__ __volatile__ (
        "sgdt (%[gdtr])\n\t"
        :
        : [gdtr] "r" (&gdtr.limit)
        : "memory"
    );
    return gdtr;
}

void cpu_set_gdtr(table_register_64_t gdtr)
{
    __asm__ __volatile__ (
        "lgdt (%[gdtr])\n\t"
        :
        : [gdtr] "r" (&gdtr.limit)
        : "memory"
    );
}

uint16_t cpu_get_tr(void)
{
    uint16_t tr;
    __asm__ __volatile__ (
        "str %w[tr]\n\t"
        : [tr] "=r" (tr)
        :
        : "memory"
    );
    return tr;
}

void cpu_set_tr(uint16_t tr)
{
    __asm__ __volatile__ (
        "ltr %w[tr]\n\t"
        :
        : [tr] "r" (tr)
        : "memory"
    );
}



uint64_t msr_adj_bit(uint32_t msr, int bit, int set)
{
    uint64_t n = msr_get(msr);
    n &= ~((uint64_t)1 << bit);
    n |= (uint64_t)!!set << bit;
    msr_set(msr, n);
    return n;
}

void *cpu_get_stack_ptr(void)
{
    void *rsp;
    __asm__ __volatile__ (
        "mov %%rsp,%[rsp]\n\t"
        : [rsp] "=r" (rsp)
    );
    return rsp;
}

void cpu_crash(void)
{
    __asm__ __volatile__ (
        "ud2"
    );
}

uint32_t cpu_get_default_mxcsr_mask(void)
{
    char save_area[512+16] = "";
    char *save_area_ptr = save_area + 15;
    uint32_t mxcsr_mask;
    __asm__ __volatile__ (
        "and $-16,%[save_area_ptr]\n\t"
        "fxsave (%[save_area_ptr])\n\t"
        "mov 0x1C(%[save_area_ptr]),%[mxcsr_mask]\n\t"
        : [mxcsr_mask] "=r" (mxcsr_mask),
          [save_area_ptr] "+r" (save_area_ptr)
    );
    return mxcsr_mask;
}
