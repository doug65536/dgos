#pragma once

#include "types.h"
#include "cpuid.h"

#define MSR_FSBASE      0xC0000100
#define MSR_GSBASE      0xC0000101
#define MSR_KGSBASE     0xC0000102
#define MSR_EFER        0xC0000080

#define MSR_IA32_MISC_ENABLES   0x1A0

// PAT MSR

#define MSR_IA32_PAT    0x277

// Uncacheable
#define MSR_IA32_PAT_UC     0

// Write Combining
#define MSR_IA32_PAT_WC     1

// Write Through
#define MSR_IA32_PAT_WT     4

// Write Protected
#define MSR_IA32_PAT_WP     5

// Writeback
#define MSR_IA32_PAT_WB     6

// Uncacheable and allow MTRR override
#define MSR_IA32_PAT_UCW    7

#define MSR_IA32_PAT_n(n,v) ((uint64_t)(v) << ((n) << 3))

#define CR0_PE_BIT 0	// Protected Mode
#define CR0_MP_BIT 1	// Monitor co-processor
#define CR0_EM_BIT 2	// Emulation
#define CR0_TS_BIT 3	// Task switched
#define CR0_ET_BIT 4	// Extension type
#define CR0_NE_BIT 5	// Numeric error
#define CR0_WP_BIT 16	// Write protect
#define CR0_AM_BIT 18	// Alignment mask
#define CR0_NW_BIT 29	// Not-write through
#define CR0_CD_BIT 30	// Cache disable
#define CR0_PG_BIT 31	// Paging

#define CR0_PE  (1 << CR0_PE_BIT)
#define CR0_MP  (1 << CR0_MP_BIT)
#define CR0_EM  (1 << CR0_EM_BIT)
#define CR0_TS  (1 << CR0_TS_BIT)
#define CR0_ET  (1 << CR0_ET_BIT)
#define CR0_NE  (1 << CR0_NE_BIT)
#define CR0_WP  (1 << CR0_WP_BIT)
#define CR0_AM  (1 << CR0_AM_BIT)
#define CR0_NW  (1 << CR0_NW_BIT)
#define CR0_CD  (1 << CR0_CD_BIT)
#define CR0_PG  (1 << CR0_PG_BIT)

#define CR4_VME_BIT         0	// Virtual 8086 Mode Extensions
#define CR4_PVI_BIT         1	// Protected-mode Virtual Interrupts
#define CR4_TSD_BIT         2	// Time Stamp Disable
#define CR4_DE_BIT          3	// Debugging Extensions
#define CR4_PSE_BIT         4	// Page Size Extension
#define CR4_PAE_BIT         5	// Physical Address Extension
#define CR4_MCE_BIT         6	// Machine Check Exception
#define CR4_PGE_BIT         7	// Page Global Enabled
#define CR4_PCE_BIT         8	// Performance-Monitoring Counter
#define CR4_OFXSR_BIT       9	// OS support for FXSAVE and FXRSTOR
#define CR4_OSXMMEX_BIT     10	// OS Support for Unmasked SIMD Floating-Point Exceptions
#define CR4_VMXE_BIT        13	// Virtual Machine Extensions Enable
#define CR4_SMXE_BIT        14	// Safer Mode Extensions Enable
#define CR4_FSGSBASE_BIT    16	// Enables instructions RDFSBASE, RDGSBASE, WRFSBASE, and WRGSBASE
#define CR4_PCIDE_BIT       17	// PCID Enable
#define CR4_OSXSAVE_BIT     18	// XSAVE
#define CR4_SMEP_BIT        20	// Supervisor Mode Execution Protection Enable
#define CR4_SMAP_BIT        21	// Supervisor Mode Access Protection Enable
#define CR4_PKE_BIT         22	// Protection Key Enable

#define CR4_VME             (1 << CR4_VME_BIT     )
#define CR4_PVI             (1 << CR4_PVI_BIT     )
#define CR4_TSD             (1 << CR4_TSD_BIT     )
#define CR4_DE              (1 << CR4_DE_BIT      )
#define CR4_PSE             (1 << CR4_PSE_BIT     )
#define CR4_PAE             (1 << CR4_PAE_BIT     )
#define CR4_MCE             (1 << CR4_MCE_BIT     )
#define CR4_PGE             (1 << CR4_PGE_BIT     )
#define CR4_PCE             (1 << CR4_PCE_BIT     )
#define CR4_OFXSR           (1 << CR4_OFXSR_BIT   )
#define CR4_OSXMMEX         (1 << CR4_OSXMMEX_BIT )
#define CR4_VMXE            (1 << CR4_VMXE_BIT    )
#define CR4_SMXE            (1 << CR4_SMXE_BIT    )
#define CR4_FSGSBASE        (1 << CR4_FSGSBASE_BIT)
#define CR4_PCIDE           (1 << CR4_PCIDE_BIT   )
#define CR4_OSXSAVE         (1 << CR4_OSXSAVE_BIT )
#define CR4_SMEP            (1 << CR4_SMEP_BIT    )
#define CR4_SMAP            (1 << CR4_SMAP_BIT    )
#define CR4_PKE             (1 << CR4_PKE_BIT     )

//
// XSAVE/XRSTOR

#define XCR0_X87_BIT            0   // x87 FPU state
#define XCR0_SSE_BIT            1   // SSE state
#define XCR0_AVX_BIT            2   // AVX state
#define XCR0_MPX_BNDREG_BIT     3   // Memory Protection BNDREGS
#define XCR0_MPX_BNDCSR_BIT     4   // Memory Protection BNDCSR
#define XCR0_AVX512_OPMASK_BIT  5   // AVX-512 opmask registers k0-k7
#define XCR0_AVX512_UPPER_BIT   6   // AVX-512 upper 256 bits
#define XCR0_AVX512_XREGS_BIT   7   // AVX-512 extra 16 registers
#define XCR0_PT_BIT             8   // Processor Trace MSRs
#define XCR0_PKRU_BIT           9   // Protection Key

#define XCR0_X87                (1<<XCR0_X87_BIT)
#define XCR0_SSE                (1<<XCR0_SSE_BIT)
#define XCR0_AVX                (1<<XCR0_AVX_BIT)
#define XCR0_MPX_BNDREG         (1<<XCR0_MPX_BNDREG_BIT)
#define XCR0_MPX_BNDCSR         (1<<XCR0_MPX_BNDCSR_BIT)
#define XCR0_AVX512_OPMASK      (1<<XCR0_AVX512_OPMASK_BIT)
#define XCR0_AVX512_UPPER       (1<<XCR0_AVX512_UPPER_BIT)
#define XCR0_AVX512_XREGS       (1<<XCR0_AVX512_XREGS_BIT)
#define XCR0_PT                 (1<<XCR0_PT_BIT)
#define XCR0_PKRU               (1<<XCR0_PKRU_BIT)

struct table_register_t {
    uint16_t align;
    uint16_t limit;
    uint32_t base;
};

struct table_register_64_t {
    // Dummy 16-bit field for alignment
    uint8_t align[sizeof(uintptr_t)-sizeof(uint16_t)];

    // Actual beginning of register value
    uint16_t limit;
    uintptr_t base;
};

__always_inline
static inline uint64_t msr_get(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ __volatile__ (
        "rdmsr\n\t"
        : "=a" (lo)
        , "=d" (hi)
        : "c" (msr)
    );
    return ((uint64_t)hi << 32) | lo;
}

__always_inline
static inline uint32_t msr_get_lo(uint32_t msr)
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

__always_inline
static inline uint32_t msr_get_hi(uint32_t msr)
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

__always_inline
static inline void msr_set(uint32_t msr, uint64_t value)
{
    __asm__ __volatile__ (
        "wrmsr\n\t"
        :
        : "a" (value)
        , "d" (value >> 32)
        , "c" (msr)
    );
}

__always_inline
static inline void msr_set_lo(uint32_t msr, uint32_t value)
{
    __asm__ __volatile__ (
        "rdmsr\n\t"
        "mov %k[value],%%eax\n\t"
        "wrmsr"
        :
        : [value] "S" (value)
        , "c" (msr)
        : "rdx"
    );
}

__always_inline
static inline void msr_set_hi(uint32_t msr, uint32_t value)
{
    __asm__ __volatile__ (
        "rdmsr\n\t"
        "mov %k[value],%%edx\n\t"
        "wrmsr"
        :
        : [value] "S" (value)
        , "c" (msr)
        : "rdx"
    );
}

__always_inline
static inline uint64_t msr_adj_bit(uint32_t msr, int bit, int set)
{
    uint64_t n = msr_get(msr);
    n &= ~((uint64_t)1 << bit);
    n |= (uint64_t)(set != 0) << bit;
    msr_set(msr, n);
    return n;
}

__always_inline
static inline uint64_t cpu_xcr_change_bits(
        uint32_t xcr, uint64_t clear, uint64_t set)
{
    uint32_t eax;
    uint32_t edx;
    __asm__ __volatile__ (
        "xgetbv\n\t"
        "shl $32,%%rdx\n\t"
        "or %%rdx,%%rax\n\t"
        "and %[clear_mask],%%rax\n\t"
        "or %[set],%%rax\n\t"
        "mov %%rax,%%rdx\n\t"
        "shr $32,%%rdx\n\t"
        "xsetbv\n\t"
        : "=a" (eax), "=d" (edx)
        : "c" (xcr)
        , [set] "S" (set)
        , [clear_mask] "D" (~clear)
        : "memory"
    );
    return ((uint64_t)edx << 32) | eax;
}

__always_inline
static inline uintptr_t cpu_cr0_change_bits(uintptr_t clear, uintptr_t set)
{
    uintptr_t rax;
    __asm__ __volatile__ (
        "mov %%cr0,%q[result]\n\t"
        "and %q[clear],%q[result]\n\t"
        "or %q[set],%q[result]\n\t"
        "mov %q[result],%%cr0\n\t"
        : [result] "=&r" (rax)
        : [clear] "ri" (~clear)
        , [set] "ri" (set)
        : "memory"
    );
    return rax;
}

__always_inline
static inline uintptr_t cpu_cr4_change_bits(uintptr_t clear, uintptr_t set)
{
    uintptr_t rax;
    __asm__ __volatile__ (
        "movq %%cr4,%q[result]\n\t"
        "andq %q[clear],%q[result]\n\t"
        "orq %q[set],%q[result]\n\t"
        "movq %q[result],%%cr4\n\t"
        : [result] "=&r" (rax)
        : [clear] "ri" (~clear)
        , [set] "ri" (set)
        : "memory"
    );
    return rax;
}

#define CPU_DR7_EN_LOCAL    0x1
#define CPU_DR7_EN_GLOBAL   0x2
#define CPU_DR7_EN_MASK     0x3
#define CPU_DR7_RW_INSN     0x0
#define CPU_DR7_RW_WRITE    0x1
#define CPU_DR7_RW_IO       0x2
#define CPU_DR7_RW_RW       0x3
#define CPU_DR7_RW_MASK     0x3
#define CPU_DR7_LEN_1       0x0
#define CPU_DR7_LEN_2       0x1
#define CPU_DR7_LEN_8       0x2
#define CPU_DR7_LEN_4       0x3
#define CPU_DR7_LEN_MASK    0x3

#define CPU_DR7_BPn_VAL(n, en, rw, len) \
    (((en) << (n * 2)) | \
    ((rw) << ((n) * 4 + 16)) | \
    ((len) << ((n) * 4 + 16 + 2)))

#define CPU_DR7_BPn_MASK(n) \
    CPU_DR7_BPn_VAL((n), CPU_DR7_EN_MASK, CPU_DR7_RW_MASK, CPU_DR7_LEN_MASK)

#define CPU_DR6_BPn_OCCURED(n)  (1<<(n))
#define CPU_DR6_BD_BIT          13
#define CPU_DR6_BS_BIT          14
#define CPU_DR6_BT_BIT          15
#define CPU_DR6_RTM_BIT         16
#define CPU_DR6_BD              (1<<CPU_DR6_BD_BIT)
#define CPU_DR6_BS              (1<<CPU_DR6_BS_BIT)
#define CPU_DR6_BT              (1<<CPU_DR6_BT_BIT)
#define CPU_DR6_RTM             (1<<CPU_DR6_RTM_BIT)

template<int dr> __always_inline
static inline uintptr_t cpu_get_debug_reg()
{
    uintptr_t value;
    __asm__ __volatile__ (
        "mov %%dr%c[dr],%[value]\n\t"
        : [value] "=r" (value)
        : [dr] "i" (dr)
    );
    return value;
}

template<int dr> __always_inline
static inline void cpu_set_debug_reg(uintptr_t value)
{
    __asm__ __volatile__ (
        "mov %[value],%%dr%c[dr]\n\t"
        :
        : [value] "r" (value)
        , [dr] "i" (dr)
    );
}

template<int dr> __always_inline
static inline uintptr_t cpu_change_debug_reg(uintptr_t clear, uintptr_t set)
{
    uintptr_t value = cpu_get_debug_reg<dr>();
    value &= ~clear;
    value |= set;
    cpu_set_debug_reg<dr>(value);
    return value;
}

template<int dr> __always_inline
static inline void cpu_set_debug_breakpoint(
        uintptr_t addr, int rw, int len, int enable)
{
    constexpr uintptr_t enable_mask = ~CPU_DR7_BPn_MASK(dr);
    uintptr_t enable_value = CPU_DR7_BPn_VAL(dr, enable, rw, len);

    uintptr_t ctl = cpu_get_debug_reg<7>();

    // Disable it before changing address
    ctl &= enable_mask;
    cpu_set_debug_reg<7>(ctl);
    cpu_set_debug_reg<dr>(addr);

    // Enable it
    ctl |= enable_value;
    cpu_set_debug_reg<7>(ctl);
}

__always_inline
static inline void cpu_set_page_directory(uintptr_t addr)
{
    __asm__ __volatile__ (
        "mov %[addr],%%cr3\n\t"
        :
        : [addr] "r" (addr)
        : "memory"
    );
}

__always_inline
static inline uintptr_t cpu_get_page_directory(void)
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "mov %%cr3,%[addr]\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

__always_inline
static inline uintptr_t cpu_get_fault_address(void)
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "mov %%cr2,%[addr]\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

__always_inline
static inline void cpu_invalidate_page(uintptr_t addr)
{
    __asm__ __volatile__ (
        "invlpg %[addr]\n\t"
        :
        : [addr] "m" (*(char*)addr)
        : "memory"
    );
}

__always_inline
static inline void cpu_invalidate_pcid(
        uintptr_t type, int32_t pcid, uintptr_t addr)
{
    struct {
        int64_t pcid;
        uintptr_t addr;
    } arg __aligned(16) = {
        pcid,
        addr
    };
    __asm__ __volatile__ (
        "invpcid %[arg],%[reg]\n\t"
        :
        : [reg] "r" (type)
        , [arg] "m" (arg)
        : "memory"
    );
}

static inline void cpu_flush_tlb(void)
{
    if (cpuid_has_invpcid()) {
        //
        // Flush all with invpcid

        cpu_invalidate_pcid(2, 0, 0);
    } else if (cpuid_has_pge()) {
        //
        // Flush all global by toggling global mappings

        // Toggle PGE off and on
        cpu_cr4_change_bits(CR4_PGE, 0);
        cpu_cr4_change_bits(0, CR4_PGE);
    } else {
        //
        // Reload CR3

        cpu_set_page_directory(cpu_get_page_directory());
    }
}

__always_inline
static inline void cpu_set_fs(uint16_t selector)
{
    __asm__ __volatile__ (
        "mov %w[selector],%%fs\n\t"
        :
        : [selector] "r" (selector)
    );
}

__always_inline
static inline void cpu_set_gs(uint16_t selector)
{
    __asm__ __volatile__ (
        "mov %w[selector],%%gs\n\t"
        :
        : [selector] "r" (selector)
    );
}

__always_inline
static inline void cpu_set_fsbase(void *fs_base)
{
    msr_set(MSR_FSBASE, (uintptr_t)fs_base);
}

__always_inline
static inline void cpu_set_gsbase(void *gs_base)
{
    msr_set(MSR_GSBASE, (uintptr_t)gs_base);
}

__always_inline
static inline table_register_64_t cpu_get_gdtr(void)
{
    table_register_64_t gdtr;
    __asm__ __volatile__ (
        "sgdtq (%[gdtr])\n\t"
        :
        : [gdtr] "r" (&gdtr.limit)
        : "memory"
    );
    return gdtr;
}

__always_inline
static inline void cpu_set_gdtr(table_register_64_t gdtr)
{
    __asm__ __volatile__ (
        "lgdtq (%[gdtr])\n\t"
        :
        : [gdtr] "r" (&gdtr.limit)
        : "memory"
    );
}

__always_inline
static inline uint16_t cpu_get_tr(void)
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

__always_inline
static inline void cpu_set_tr(uint16_t tr)
{
    __asm__ __volatile__ (
        "ltr %w[tr]\n\t"
        :
        : [tr] "r" (tr)
        : "memory"
    );
}

__always_inline
static inline void *cpu_get_stack_ptr(void)
{
    void *rsp;
    __asm__ __volatile__ (
        "mov %%rsp,%[rsp]\n\t"
        : [rsp] "=r" (rsp)
    );
    return rsp;
}

__always_inline
static inline void cpu_crash(void)
{
    __asm__ __volatile__ (
        "ud2"
    );
}

__always_inline
static inline void cpu_fxsave(void *fpuctx)
{
    __asm__ __volatile__ (
        "fxsave64 (%0)\n\t"
        :
        : "r" (fpuctx)
        : "memory"
    );
}

__always_inline
static inline void cpu_xsave(void *fpuctx)
{
    __asm__ __volatile__ (
        "xsave64 (%[fpuctx])\n\t"
        :
        : "a" (-1), "d" (-1), [fpuctx] "D" (fpuctx)
        : "memory"
    );
}

__always_inline
static inline void *cpu_gs_read_ptr(void)
{
    void *ptr;
    __asm__ __volatile__ (
        "mov %%gs:0,%[ptr]\n\t"
        : [ptr] "=r" (ptr)
        :
        : "memory"
    );
    return ptr;
}

__always_inline
static inline uintptr_t cpu_get_flags(void)
{
    uintptr_t flags;
    __asm__ __volatile__ (
        "pushf\n\t"
        "pop %[flags]\n\t"
        : [flags] "=r" (flags)
    );
    return flags;
}

__always_inline
static inline void cpu_set_flags(uintptr_t flags)
{
    __asm__ __volatile__ (
        "push %[flags]\n\t"
        "popf\n\t"
        :
        : [flags] "ir" (flags)
        : "cc"
    );
}

__always_inline
static inline uintptr_t cpu_change_flags(uintptr_t clear, uintptr_t set)
{
    uintptr_t flags;
    __asm__ __volatile__ (
        "pushf\n\t"
        "pop %[flags]\n\t"
        "and %[clear],%[flags]\n\t"
        "or %[set],%[flags]\n\t"
        "push %[flags]\n\t"
        "popf"
        : [flags] "=&r" (flags)
        : [clear] "ir" (clear)
        , [set] "ir" (set)
        : "cc"
    );
    return flags;
}

__always_inline
static inline int cpu_irq_disable(void)
{
    uintptr_t rflags;
    __asm__ __volatile__ (
        "pushf\n\t"
        "pop %[rflags]\n\t"
        "cli\n\t"
        : [rflags] "=r" (rflags)
        :
        : "cc"
    );
    return ((rflags >> 9) & 1);
}

__always_inline
static inline void cpu_irq_enable(void)
{
    __asm__ __volatile__ ( "sti" : : : "cc" );
}

__always_inline
static inline void cpu_irq_toggle(int enable)
{
    uintptr_t temp;
    __asm__ __volatile__ (
        "pushfq\n\t"
        "pop %q[temp]\n\t"
        "and $~(1<<9),%k[temp]\n\t"
        "or %k[enable],%k[temp]\n\t"
        "push %q[temp]\n\t"
        "popfq\n\t"
        : [temp] "=&r" (temp)
        : [enable] "ir" ((enable != 0) << 9)
        : "cc"
    );
}

__always_inline
static inline int cpu_irq_is_enabled(void)
{
    return (cpu_get_flags() & (1<<9)) != 0;
}

__always_inline
static inline uint64_t cpu_rdtsc(void)
{
    uint32_t tsc_lo;
    uint32_t tsc_hi;
    __asm__ __volatile__ (
        "rdtsc\n\t"
        : "=a" (tsc_lo), "=d" (tsc_hi)
    );
    return tsc_lo | ((uint64_t)tsc_hi << 32);
}

//
// C++ utilities

class cpu_scoped_irq_disable
{
public:
    __always_inline
    cpu_scoped_irq_disable()
        : intr_was_enabled((cpu_irq_disable() << 1) - 1)
    {
    }

    __always_inline
    ~cpu_scoped_irq_disable()
    {
        if (intr_was_enabled)
            cpu_irq_toggle(intr_was_enabled > 0);
    }

    __always_inline
    operator bool() const
    {
        return intr_was_enabled > 0;
    }

    __always_inline
    void restore()
    {
        if (intr_was_enabled) {
            cpu_irq_toggle(intr_was_enabled > 0);
            intr_was_enabled = 0;
        }
    }

private:
    int8_t intr_was_enabled;
};


extern "C" __noinline
void cpu_debug_break();
