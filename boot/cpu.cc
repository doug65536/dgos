#include "cpu.h"
#include "paging.h"
#include "gdt_sel.h"
#include "screen.h"

extern gdt_entry_t gdt[];

table_register_64_t idtr_64;
table_register_t idtr_16 __used = {
    0,
    4 * 256,
    0
};

idt_entry_64_t idt[32];

static __always_inline uintptr_t cpu_cr4_change_bits(
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

static __always_inline uint64_t cpu_xcr_change_bits(
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

const char *cpu_choose_kernel()
{
//    if (cpu_has_bmi())
//        return "dgos-kernel-bmi";
//    else
        return "dgos-kernel-generic";
}

bool nx_available;
uint32_t gp_available;

void cpu_init()
{
    nx_available = cpu_has_no_execute();
    gp_available = cpu_has_global_pages() ? (1 << 7) : 0;
}

// 64-bit assembly code

extern "C" void code64_run_kernel(void *p);
extern "C" void code64_reloc_kernel(void *p);
extern "C" void code64_copy_kernel(void *p);

void reloc_kernel(uint64_t distance, void *elf_rela, size_t relcnt)
{
    struct {
        uint64_t distance;
        void *elf_rela;
        size_t relcnt;
    } arg = {
        distance,
        elf_rela,
        relcnt
    };

    run_code64(code64_reloc_kernel, &arg);
}

void run_kernel(uint64_t entry, void *param)
{
    struct {
        uint64_t entry;
        void *param;
    } arg = {
        entry,
        param
    };

    run_code64(code64_run_kernel, &arg);
}

void copy_kernel(uint64_t dest_addr, void *src, size_t sz)
{
    struct {
        uint64_t dest_addr;
        void *src;
        size_t sz;
    } arg = {
        dest_addr,
        src,
        sz
    };

    run_code64(code64_copy_kernel, &arg);
}

