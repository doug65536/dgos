#include "cpu.h"
#include "paging.h"
#include "gdt_sel.h"
#include "screen.h"

extern gdt_entry_t gdt[];

table_register_64_t idtr_64;
table_register_t idtr_16 _used = {
    0,
    4 * 256,
    0
};

idt_entry_64_t idt[32];

static _always_inline uintptr_t cpu_cr4_change_bits(
        uintptr_t clear, uintptr_t set)
{
    uintptr_t rax;
    __asm__ __volatile__ (
        "mov %%cr4,%[result]\n\t"
        "and %[clear],%[result]\n\t"
        "or %[set],%[result]\n\t"
        "mov %[result],%%cr4\n\t"
        : [result] "=&r" (rax)
        : [clear] "ri" (~clear)
        , [set] "ri" (set)
        : "memory"
    );
    return rax;
}

static _always_inline uint64_t cpu_xcr_change_bits(
        uint32_t xcr, uint64_t clear, uint64_t set)
{
    clear = ~clear;
    uint64_t result;
    __asm__ __volatile__ (
        "xgetbv\n\t"
        "and (%[clear_mask]),%%eax\n\t"
        "and 4(%[clear_mask]),%%edx\n\t"
        "or (%[set]),%%eax\n\t"
        "or 4(%[set]),%%edx\n\t"
        "xsetbv\n\t"
        : "=&A" (result)
        : "c" (xcr)
        , [set] "r" (&set)
        , [clear_mask] "r" (&clear)
        : "memory"
    );
    return result;
}

//#define XCR0_X87_BIT            0   // x87 FPU state
//#define XCR0_SSE_BIT            1   // SSE state
//#define XCR0_AVX_BIT            2   // AVX state
//#define XCR0_MPX_BNDREG_BIT     3   // Memory Protection BNDREGS
//#define XCR0_MPX_BNDCSR_BIT     4   // Memory Protection BNDCSR
//#define XCR0_AVX512_OPMASK_BIT  5   // AVX-512 opmask registers k0-k7
//#define XCR0_AVX512_UPPER_BIT   6   // AVX-512 upper 256 bits
//#define XCR0_AVX512_XREGS_BIT   7   // AVX-512 extra 16 registers
//#define XCR0_PT_BIT             8   // Processor Trace MSRs
//#define XCR0_PKRU_BIT           9   // Protection Key

//#define XCR0_X87                (1<<XCR0_X87_BIT)
//#define XCR0_SSE                (1<<XCR0_SSE_BIT)
//#define XCR0_AVX                (1<<XCR0_AVX_BIT)
//#define XCR0_MPX_BNDREG         (1<<XCR0_MPX_BNDREG_BIT)
//#define XCR0_MPX_BNDCSR         (1<<XCR0_MPX_BNDCSR_BIT)
//#define XCR0_AVX512_OPMASK      (1<<XCR0_AVX512_OPMASK_BIT)
//#define XCR0_AVX512_UPPER       (1<<XCR0_AVX512_UPPER_BIT)
//#define XCR0_AVX512_XREGS       (1<<XCR0_AVX512_XREGS_BIT)
//#define XCR0_PT                 (1<<XCR0_PT_BIT)
//#define XCR0_PKRU               (1<<XCR0_PKRU_BIT)

_section(".smp.data") bool nx_available;
uint32_t gp_available;

void cpu_init()
{
    nx_available = cpu_has_no_execute();
    gp_available = cpu_has_global_pages() ? CPU_CR4_PGE : 0;
}
