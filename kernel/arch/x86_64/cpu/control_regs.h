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

static __always_inline uint64_t cpu_msr_get(uint32_t msr)
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

static __always_inline uint32_t cpu_msr_get_lo(uint32_t msr)
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

static __always_inline uint32_t cpu_msr_get_hi(uint32_t msr)
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

static __always_inline void cpu_msr_set(uint32_t msr, uint64_t value)
{
    __asm__ __volatile__ (
        "wrmsr\n\t"
        :
        : "a" (value)
        , "d" (value >> 32)
        , "c" (msr)
    );
}

static __always_inline void cpu_msr_set_lo(uint32_t msr, uint32_t value)
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

static __always_inline void cpu_msr_set_hi(uint32_t msr, uint32_t value)
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

static __always_inline uint64_t cpu_msr_change_bits(
        uint32_t msr, uint64_t clr, uint64_t set)
{
    uint64_t n = cpu_msr_get(msr);
    n &= ~clr;
    n |= set;
    cpu_msr_set(msr, n);
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

static __always_inline uintptr_t cpu_get_cr8()
{
    uintptr_t cr8;
    __asm__ __volatile__ (
        "mov %%cr8,%[cr8]\n\t"
        : [cr8] "=r" (cr8)
    );
    return cr8;
}

static __always_inline void cpu_set_cr8(uintptr_t cr8)
{
    __asm__ __volatile__ (
        "mov %[cr8],%%cr8\n\t"
        :
        : [cr8] "r" (cr8)
    );
}

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

static __always_inline uintptr_t cpu_get_page_directory()
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "mov %%cr3,%[addr]\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

static __always_inline uintptr_t cpu_get_fault_address()
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

static __always_inline void cpu_flush_tlb()
{
    if (cpuid_has_invpcid()) {
        //
        // Flush all with invpcid

        cpu_invalidate_pcid(2, 0, 0);
    } else if (cpuid_has_pge()) {
        //
        // Flush all global by toggling global mappings

        // Toggle PGE off and on
        cpu_cr4_change_bits(CPU_CR4_PGE, 0);
        cpu_cr4_change_bits(0, CPU_CR4_PGE);
    } else {
        //
        // Reload CR3

        cpu_set_page_directory(cpu_get_page_directory());
    }
}

static __always_inline void cpu_flush_cache()
{
    __asm__ __volatile__ ("wbinvd\n\t" : : : "memory");
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
    cpu_msr_set(CPU_MSR_FSBASE, (uintptr_t)fs_base);
}

static __always_inline void cpu_set_gsbase(void *gs_base)
{
    cpu_msr_set(CPU_MSR_GSBASE, (uintptr_t)gs_base);
}

static __always_inline void cpu_set_altgsbase(void *gs_base)
{
    cpu_msr_set(CPU_MSR_KGSBASE, (uintptr_t)gs_base);
}

static __always_inline table_register_64_t cpu_get_gdtr()
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

static __always_inline uint16_t cpu_get_tr()
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

static __always_inline void cpu_set_ldt(uint16_t ldt)
{
    __asm__ __volatile__ (
        "lldt %w[ldt]\n\t"
        :
        : [ldt] "r" (ldt)
    );
}

static __always_inline void *cpu_get_stack_ptr()
{
    void *rsp;
    __asm__ __volatile__ (
        "mov %%rsp,%[rsp]\n\t"
        : [rsp] "=r" (rsp)
    );
    return rsp;
}

static __always_inline void cpu_crash()
{
    __asm__ __volatile__ (
        "ud2"
    );
}

static __always_inline void cpu_breakpoint()
{
    __asm__ __volatile__ (
        "int3"
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

static __always_inline uint32_t cpu_get_eflags()
{
    uint32_t eflags;
    __asm__ __volatile__ (
        "pushf\n\t"
        "pop %q[eflags]\n\t"
        : [eflags] "=r" (eflags)
    );
    return eflags;
}

static __always_inline void cpu_set_eflags(uint32_t flags)
{
    __asm__ __volatile__ (
        "push %[flags]\n\t"
        "popf\n\t"
        :
        : [flags] "ir" (flags)
        : "cc"
    );
}

static __always_inline uint32_t cpu_change_eflags(
        uint32_t clear, uint32_t set)
{
    uint32_t flags;
    __asm__ __volatile__ (
        "pushfq\n\t"
        "popq %q[flags]\n\t"
        "andq %q[clear],%q[flags]\n\t"
        "orq %q[set],%q[flags]\n\t"
        "pushq %q[flags]\n\t"
        "popfq"
        : [flags] "=&r" (flags)
        : [clear] "ir" (clear)
        , [set] "ir" (set)
        : "cc"
    );
    return flags;
}

static __always_inline void cpu_stac()
{
    __asm__ __volatile__ ("stac");
}

static __always_inline void cpu_clac()
{
    __asm__ __volatile__ ("clac");
}

static __always_inline void cpu_irq_disable()
{
    __asm__ __volatile__ (
        "cli\n\t"
    );
}

static __always_inline bool cpu_irq_save_disable()
{
    uint32_t eflags;
    __asm__ __volatile__ (
        "pushfq\n\t"
        "popq %q[eflags]\n\t"
        "cli\n\t"
        : [eflags] "=r" (eflags)
        :
        : "cc"
    );
    return eflags & CPU_EFLAGS_IF;
}

static __always_inline void cpu_irq_enable()
{
    __asm__ __volatile__ ( "sti" : : : "cc" );
}

static __always_inline void cpu_irq_toggle(bool enable)
{
    uint32_t temp;
    __asm__ __volatile__ (
        "pushfq\n\t"
        "popq %q[temp]\n\t"
        "andl %[not_eflags_if],%k[temp]\n\t"
        "orl %k[enable],%k[temp]\n\t"
        "pushq %q[temp]\n\t"
        "popfq\n\t"
        : [temp] "=&r" (temp)
        : [enable] "ir" (enable << CPU_EFLAGS_IF_BIT)
        , [not_eflags_if] "i" (~CPU_EFLAGS_IF)
        : "cc"
    );
}

static __always_inline bool cpu_irq_is_enabled()
{
    return cpu_get_eflags() & CPU_EFLAGS_IF;
}

static __always_inline uint64_t cpu_rdtsc()
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
        : intr_was_enabled((cpu_irq_save_disable() << 1) - 1)
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
    __asm__ __volatile__ (
        "monitor"
        :
        : "a" (addr)
        , "c" (ext)
        , "d" (hint)
        : "memory"
    );
}

static __always_inline void cpu_mwait(uint32_t ext, uint32_t hint)
{
    __asm__ __volatile__ (
        "mwait"
        :
        : "c" (ext)
        , "d" (hint)
        : "memory"
    );
}

// is_equal: true to wait for value, false to wait for not equal to value
// value: address to watch
// wait_value: comparison value
// mask: masks value before comparison
template<typename T>
static __always_inline void cpu_wait_masked(
        bool is_equal, T const volatile *value, T wait_value, T mask)
{
    if (cpuid_has_mwait()) {
        while (is_equal != ((*value & mask) == wait_value)) {
            cpu_monitor(value, 0, 0);

            if (is_equal != ((*value & mask) == wait_value))
                cpu_mwait(0, 0);
        }
    } else {
        while (is_equal != ((*value & mask) == wait_value))
            pause();
    }
}

// is_equal: true to wait for value, false to wait for not equal to value
// value: address to watch
// wait_value: comparison value
template<typename T>
static __always_inline void cpu_wait_unmasked(
        bool is_equal, T const volatile *value, T wait_value)
{
    if (cpuid_has_mwait()) {
        while (is_equal != (*value == wait_value)) {
            cpu_monitor(value, 0, 0);

            if (is_equal != (*value == wait_value))
                cpu_mwait(0, 0);
        }
    } else {
        while (is_equal != (*value == wait_value))
            pause();
    }
}

template<typename T>
static __always_inline void cpu_wait_value(
        T const volatile *value, T wait_value)
{
    return cpu_wait_unmasked(true, value, wait_value);
}

template<typename T>
static __always_inline void cpu_wait_not_value(
        T const volatile *value, T wait_value)
{
    return cpu_wait_unmasked(false, value, wait_value);
}


template<typename T>
static __always_inline void cpu_wait_value(
        T const volatile *value, T wait_value, T mask)
{
    return cpu_wait_masked(true, value, wait_value, mask);
}

template<typename T>
static __always_inline void cpu_wait_not_value(
        T const volatile *value, T wait_value, T mask)
{
    return cpu_wait_masked(false, value, wait_value, mask);
}

template<typename T>
static __always_inline void cpu_wait_bit_value(
        T const volatile *value, uint8_t bit, bool bit_value)
{
    return cpu_wait_value(value, T(bit_value) << bit, T(1) << bit);
}

template<typename T>
static __always_inline void cpu_wait_bit_clear(
        T const volatile *value, uint8_t bit)
{
    return cpu_wait_bit_value(value, bit, false);
}

template<typename T>
static __always_inline void cpu_wait_bit_set(
        T const volatile *value, uint8_t bit)
{
    return cpu_wait_bit_value(value, bit, true);
}

extern "C" __noinline
void cpu_debug_break();
