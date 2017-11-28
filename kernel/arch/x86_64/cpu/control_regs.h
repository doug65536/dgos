#pragma once

#include "types.h"
#include "cpuid.h"
#include "atomic.h"

#include "control_regs_constants.h"

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

static __always_inline uint64_t msr_get(uint32_t msr)
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

static __always_inline uint32_t msr_get_lo(uint32_t msr)
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

static __always_inline uint32_t msr_get_hi(uint32_t msr)
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

static __always_inline void msr_set(uint32_t msr, uint64_t value)
{
    __asm__ __volatile__ (
        "wrmsr\n\t"
        :
        : "a" (value)
        , "d" (value >> 32)
        , "c" (msr)
    );
}

static __always_inline void msr_set_lo(uint32_t msr, uint32_t value)
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

static __always_inline void msr_set_hi(uint32_t msr, uint32_t value)
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

static __always_inline uint64_t msr_adj_bit(uint32_t msr, int bit, int set)
{
    uint64_t n = msr_get(msr);
    n &= ~((uint64_t)1 << bit);
    n |= (uint64_t)(set != 0) << bit;
    msr_set(msr, n);
    return n;
}

static __always_inline uint64_t cpu_xcr_change_bits(
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

static __always_inline uintptr_t cpu_cr0_change_bits(
        uintptr_t clear, uintptr_t set)
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

static __always_inline uintptr_t cpu_cr4_change_bits(
        uintptr_t clear, uintptr_t set)
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

template<int dr>
static __always_inline uintptr_t cpu_get_debug_reg()
{
    uintptr_t value;
    __asm__ __volatile__ (
        "mov %%dr%c[dr],%[value]\n\t"
        : [value] "=r" (value)
        : [dr] "i" (dr)
    );
    return value;
}

template<int dr>
static __always_inline void cpu_set_debug_reg(uintptr_t value)
{
    __asm__ __volatile__ (
        "mov %[value],%%dr%c[dr]\n\t"
        :
        : [value] "r" (value)
        , [dr] "i" (dr)
    );
}

template<int dr>
static __always_inline uintptr_t cpu_change_debug_reg(
        uintptr_t clear, uintptr_t set)
{
    uintptr_t value = cpu_get_debug_reg<dr>();
    value &= ~clear;
    value |= set;
    cpu_set_debug_reg<dr>(value);
    return value;
}

template<int dr>
static __always_inline void cpu_set_debug_breakpoint(
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

void cpu_set_debug_breakpoint_indirect(uintptr_t addr, int rw,
                                       int len, int enable, size_t index);

static __always_inline void cpu_set_page_directory(uintptr_t addr)
{
    __asm__ __volatile__ (
        "mov %[addr],%%cr3\n\t"
        :
        : [addr] "r" (addr)
        : "memory"
    );
}

static __always_inline uintptr_t cpu_get_page_directory(void)
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "mov %%cr3,%[addr]\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

static __always_inline uintptr_t cpu_get_fault_address(void)
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "mov %%cr2,%[addr]\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

static __always_inline void cpu_invalidate_page(uintptr_t addr)
{
    __asm__ __volatile__ (
        "invlpg %[addr]\n\t"
        :
        : [addr] "m" (*(char*)addr)
        : "memory"
    );
}

static __always_inline void cpu_invalidate_pcid(
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

static __always_inline void cpu_flush_tlb(void)
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

static __always_inline void cpu_set_fs(uint16_t selector)
{
    __asm__ __volatile__ (
        "mov %w[selector],%%fs\n\t"
        :
        : [selector] "r" (selector)
    );
}

static __always_inline void cpu_set_gs(uint16_t selector)
{
    __asm__ __volatile__ (
        "mov %w[selector],%%gs\n\t"
        :
        : [selector] "r" (selector)
    );
}

static __always_inline void cpu_set_fsbase(void *fs_base)
{
    msr_set(MSR_FSBASE, (uintptr_t)fs_base);
}

static __always_inline void cpu_set_gsbase(void *gs_base)
{
    msr_set(MSR_GSBASE, (uintptr_t)gs_base);
}

static __always_inline table_register_64_t cpu_get_gdtr(void)
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

static __always_inline void cpu_set_gdtr(table_register_64_t gdtr)
{
    __asm__ __volatile__ (
        "lgdtq (%[gdtr])\n\t"
        :
        : [gdtr] "r" (&gdtr.limit)
        : "memory"
    );
}

static __always_inline uint16_t cpu_get_tr(void)
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

static __always_inline void cpu_set_tr(uint16_t tr)
{
    __asm__ __volatile__ (
        "ltr %w[tr]\n\t"
        :
        : [tr] "r" (tr)
        : "memory"
    );
}

static __always_inline void *cpu_get_stack_ptr(void)
{
    void *rsp;
    __asm__ __volatile__ (
        "mov %%rsp,%[rsp]\n\t"
        : [rsp] "=r" (rsp)
    );
    return rsp;
}

static __always_inline void cpu_crash(void)
{
    __asm__ __volatile__ (
        "ud2"
    );
}

static __always_inline void cpu_fxsave(void *fpuctx)
{
    __asm__ __volatile__ (
        "fxsave64 (%0)\n\t"
        :
        : "r" (fpuctx)
        : "memory"
    );
}

static __always_inline void cpu_xsave(void *fpuctx)
{
    __asm__ __volatile__ (
        "xsave64 (%[fpuctx])\n\t"
        :
        : "a" (-1), "d" (-1), [fpuctx] "D" (fpuctx)
        : "memory"
    );
}

static __always_inline uintptr_t cpu_get_flags(void)
{
    uintptr_t flags;
    __asm__ __volatile__ (
        "pushf\n\t"
        "pop %[flags]\n\t"
        : [flags] "=r" (flags)
    );
    return flags;
}

static __always_inline void cpu_set_flags(uintptr_t flags)
{
    __asm__ __volatile__ (
        "push %[flags]\n\t"
        "popf\n\t"
        :
        : [flags] "ir" (flags)
        : "cc"
    );
}

static __always_inline uintptr_t cpu_change_flags(
        uintptr_t clear, uintptr_t set)
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

static __always_inline int cpu_irq_disable(void)
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

static __always_inline void cpu_irq_enable(void)
{
    __asm__ __volatile__ ( "sti" : : : "cc" );
}

static __always_inline void cpu_irq_toggle(int enable)
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

static __always_inline int cpu_irq_is_enabled(void)
{
    return (cpu_get_flags() & (1<<9)) != 0;
}

static __always_inline uint64_t cpu_rdtsc(void)
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

// Monitor/mwait

template<typename T>
static __always_inline void cpu_monitor(
        T const volatile *addr, uint32_t ext, uint32_t hint)
{
    static_assert(sizeof(T) <= sizeof(uint64_t), "Questionable size");

    __asm__ __volatile__ (
        "monitor"
        :
        : "a" (addr)
        , "c" (ext)
        , "d" (hint)
    );
}

static __always_inline void cpu_mwait(uint32_t ext, uint32_t hint)
{
    __asm__ __volatile__ (
        "mwait"
        :
        : "c" (ext)
        , "d" (hint)
    );
}

template<typename T>
static __always_inline void cpu_wait_bit_clear(
        T const volatile *value, uint8_t bit)
{
    static_assert(sizeof(T) <= sizeof(uint64_t), "Questionable size");

    T mask = T(1) << bit;
    if (cpuid_has_mwait()) {
        while (*value & mask) {
            cpu_monitor(value, 0, 0);

            if (*value & mask)
                cpu_mwait(0, 0);
        }
    } else {
        while (*value & mask)
            pause();
    }
}

template<typename T>
static __always_inline void cpu_wait_value(
        T const volatile *value, T wait_value)
{
    static_assert(sizeof(T) <= sizeof(uint64_t), "Questionable size");

    if (cpuid_has_mwait()) {
        while (*value != wait_value) {
            cpu_monitor(value, 0, 0);

            if (*value != wait_value)
                cpu_mwait(0, 0);
        }
    } else {
        while (*value != wait_value)
            pause();
    }
}

extern "C" __noinline
void cpu_debug_break();
