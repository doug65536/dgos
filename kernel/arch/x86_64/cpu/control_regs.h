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

static _always_inline uint64_t cpu_msr_get(uint32_t msr)
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

static _always_inline uint32_t cpu_msr_get_lo(uint32_t msr)
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

static _always_inline uint32_t cpu_msr_get_hi(uint32_t msr)
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

static _always_inline void cpu_msr_set(uint32_t msr, uint64_t value)
{
    __asm__ __volatile__ (
        "wrmsr\n\t"
        :
        : "a" (value)
        , "d" (value >> 32)
        , "c" (msr)
    );
}

static _always_inline void cpu_msr_lo_set(uint32_t msr, uint32_t value)
{
    __asm__ __volatile__ (
        "rdmsr\n\t"
        "movl %k[value],%%eax\n\t"
        "wrmsr"
        :
        : [value] "S" (value)
        , "c" (msr)
        : "rdx"
    );
}

static _always_inline void cpu_msr_hi_set(uint32_t msr, uint32_t value)
{
    __asm__ __volatile__ (
        "rdmsr\n\t"
        "movl %k[value],%%edx\n\t"
        "wrmsr"
        :
        : [value] "S" (value)
        , "c" (msr)
        : "rdx"
    );
}

static _always_inline uint64_t cpu_msr_change_bits(
        uint32_t msr, uint64_t clr, uint64_t set)
{
    uint64_t n = cpu_msr_get(msr);
    n &= ~clr;
    n |= set;
    cpu_msr_set(msr, n);
    return n;
}

static _always_inline uint64_t cpu_xcr_change_bits(
        uint32_t xcr, uint64_t clear, uint64_t set)
{
    uint32_t eax;
    uint32_t edx;
    __asm__ __volatile__ (
        "xgetbv\n\t"
        "shlq $32,%%rdx\n\t"
        "orq %%rdx,%%rax\n\t"
        "andq %[clear_mask],%%rax\n\t"
        "orq %[set],%%rax\n\t"
        "movq %%rax,%%rdx\n\t"
        "shrq $32,%%rdx\n\t"
        "xsetbv\n\t"
        : "=a" (eax), "=d" (edx)
        : "c" (xcr)
        , [set] "S" (set)
        , [clear_mask] "D" (~clear)
        : "memory"
    );
    return ((uint64_t)edx << 32) | eax;
}

static _always_inline uintptr_t cpu_cr0_get()
{
    uintptr_t rax;
    __asm__ __volatile__ (
        "mov %%cr0,%q[result]\n\t"
        : [result] "=r" (rax)
    );
    return rax;
}

static _always_inline void cpu_cr0_set(uintptr_t cr0)
{
    __asm__ __volatile__ (
        "mov %q[cr0],%%cr0\n\t"
        :
        : [cr0] "r" (cr0)
    );
}

static _always_inline void cpu_cr0_clts()
{
    __asm__ __volatile__ (
        "clts\n\t"
    );
}

static _always_inline uintptr_t cpu_cr0_change_bits(
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

static _always_inline uintptr_t cpu_cr4_change_bits(
        uintptr_t clear, uintptr_t set)
{
    uintptr_t r;
    __asm__ __volatile__ (
        "movq %%cr4,%q[result]\n\t"
        "andq %q[clear],%q[result]\n\t"
        "orq %q[set],%q[result]\n\t"
        "movq %q[result],%%cr4\n\t"
        : [result] "=&r" (r)
        : [clear] "ri" (~clear)
        , [set] "ri" (set)
        : "memory"
    );
    return r;
}

static _always_inline uintptr_t cpu_cr8_get()
{
    uintptr_t cr8;
    __asm__ __volatile__ (
        "movq %%cr8,%[cr8]\n\t"
        : [cr8] "=r" (cr8)
    );
    return cr8;
}

static _always_inline void cpu_cr8_set(uintptr_t cr8)
{
    __asm__ __volatile__ (
        "movq %[cr8],%%cr8\n\t"
        :
        : [cr8] "r" (cr8)
    );
}

template<int dr>
static _always_inline uintptr_t cpu_debug_reg_get()
{
    uintptr_t value;
    __asm__ __volatile__ (
        "movq %%dr%c[dr],%[value]\n\t"
        : [value] "=r" (value)
        : [dr] "i" (dr)
    );
    return value;
}

template<int dr>
static _always_inline void cpu_debug_reg_set(uintptr_t value)
{
    __asm__ __volatile__ (
        "movq %[value],%%dr%c[dr]\n\t"
        :
        : [value] "r" (value)
        , [dr] "i" (dr)
    );
}

template<int dr>
static _always_inline uintptr_t cpu_debug_reg_change(
        uintptr_t clear, uintptr_t set)
{
    uintptr_t value = cpu_debug_reg_get<dr>();
    value &= ~clear;
    value |= set;
    cpu_debug_reg_set<dr>(value);
    return value;
}

template<int dr>
static _always_inline void cpu_debug_breakpoint_set(
        uintptr_t addr, int rw, int len, int enable)
{
    constexpr uintptr_t enable_mask = ~CPU_DR7_BPn_MASK(dr);
    uintptr_t enable_value = CPU_DR7_BPn_VAL(dr, enable, rw, len);

    uintptr_t ctl = cpu_debug_reg_get<7>();

    // Disable it before changing address
    ctl &= enable_mask;
    cpu_debug_reg_set<7>(ctl);
    cpu_debug_reg_set<dr>(addr);

    // Enable it
    ctl |= enable_value;
    cpu_debug_reg_set<7>(ctl);
}

void cpu_debug_breakpoint_set_indirect(uintptr_t addr, int rw,
                                       int len, int enable, size_t index);

static _always_inline void cpu_page_directory_set(uintptr_t addr)
{
    __asm__ __volatile__ (
        "movq %[addr],%%cr3\n\t"
        :
        : [addr] "r" (addr)
        : "memory"
    );
}

static _always_inline uintptr_t cpu_page_directory_get()
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "movq %%cr3,%[addr]\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

static _always_inline uintptr_t cpu_fault_address_get()
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "mov %%cr2,%[addr]\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

static _always_inline void cpu_fault_address_set(uintptr_t addr)
{
    __asm__ __volatile__ (
        "mov %[addr],%%cr2\n\t"
        :
        : [addr] "r" (addr)
    );
}

static _always_inline void cpu_page_invalidate(uintptr_t addr)
{
    __asm__ __volatile__ (
        "invlpg %[addr]\n\t"
        :
        : [addr] "m" (*(char*)addr)
        : "memory"
    );
}

static _always_inline void cpu_pcid_invalidate(
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

static _always_inline void cpu_tlb_flush()
{
    if (cpuid_has_invpcid()) {
        //
        // Flush all with invpcid

        cpu_pcid_invalidate(2, 0, 0);
    } else if (cpuid_has_pge()) {
        //
        // Flush all global by toggling global mappings

        // Toggle PGE off and on
        cpu_cr4_change_bits(CPU_CR4_PGE, 0);
        cpu_cr4_change_bits(0, CPU_CR4_PGE);
    } else {
        //
        // Reload CR3

        cpu_page_directory_set(cpu_page_directory_get());
    }
}

static _always_inline void cpu_cache_flush()
{
    __asm__ __volatile__ ("wbinvd\n\t" : : : "memory");
}

static _always_inline void cpu_ds_set(uint16_t selector)
{
    __asm__ __volatile__ (
        "movw %w[selector],%%ds\n\t"
        :
        : [selector] "r" (selector)
    );
}

static _always_inline void cpu_es_set(uint16_t selector)
{
    __asm__ __volatile__ (
        "movw %w[selector],%%es\n\t"
        :
        : [selector] "r" (selector)
    );
}

static _always_inline void cpu_fs_set(uint16_t selector)
{
    __asm__ __volatile__ (
        "movw %w[selector],%%fs\n\t"
        :
        : [selector] "r" (selector)
    );
}

static _always_inline void cpu_gs_set(uint16_t selector)
{
    __asm__ __volatile__ (
        "movw %w[selector],%%gs\n\t"
        :
        : [selector] "r" (selector)
    );
}

static _always_inline void cpu_ss_set(uint16_t selector)
{
    __asm__ __volatile__ (
        "movw %w[selector],%%ss\n\t"
        :
        : [selector] "r" (selector)
    );
}

static _always_inline void cpu_cs_set(uint16_t selector)
{
    uint64_t temp;
    __asm__ __volatile__ (
        "leaq 0f(%%rip),%[temp]\n\t"
        "pushq %q[selector]\n\t"
        "pushq %q[temp]\n\t"
        "lretq\n\t"
        "0:\n\t"
        : [temp] "=&r" (temp)
        : [selector] "r" (selector)
    );
}

static _always_inline void cpu_fsbase_set(void *fs_base)
{
    cpu_msr_set(CPU_MSR_FSBASE, (uintptr_t)fs_base);
}

static _always_inline void cpu_gsbase_set(void *gs_base)
{
    cpu_msr_set(CPU_MSR_GSBASE, (uintptr_t)gs_base);
}

static _always_inline void cpu_altgsbase_set(void *gs_base)
{
    cpu_msr_set(CPU_MSR_KGSBASE, (uintptr_t)gs_base);
}

static _always_inline table_register_64_t cpu_gdtr_get()
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

static _always_inline void cpu_gdtr_set(table_register_64_t gdtr)
{
    __asm__ __volatile__ (
        "lgdtq (%[gdtr])\n\t"
        :
        : [gdtr] "r" (&gdtr.limit)
        : "memory"
    );
}

static _always_inline table_register_64_t cpu_idtr_get()
{
    table_register_64_t idtr{};
    __asm__ __volatile__ (
        "sidtq (%[idtr])\n\t"
        :
        : [idtr] "r" (&idtr.limit)
        : "memory"
    );
    return idtr;
}

static _always_inline void cpu_idtr_set(table_register_64_t idtr)
{
    __asm__ __volatile__ (
        "lidtq (%[idtr])\n\t"
        :
        : [idtr] "r" (&idtr.limit)
        : "memory"
    );
}

static _always_inline uint16_t cpu_tr_get()
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

static _always_inline void cpu_tr_set(uint16_t tr)
{
    __asm__ __volatile__ (
        "ltr %w[tr]\n\t"
        :
        : [tr] "r" (tr)
        : "memory"
    );
}

static _always_inline void cpu_ldt_set(uint16_t ldt)
{
    __asm__ __volatile__ (
        "lldtw %w[ldt]\n\t"
        :
        : [ldt] "r" (ldt)
    );
}

static _always_inline void cpu_mxcsr_set(uint32_t mxcsr)
{
    __asm__ __volatile__ (
        "ldmxcsr %[mxcsr]\n\t"
        :
        : [mxcsr] "m" (mxcsr)
    );
}

static _always_inline uint32_t cpu_mxcsr_get()
{
    uint32_t mxcsr;
    __asm__ __volatile__ (
        "stmxcsr %[mxcsr]\n\t"
        : [mxcsr] "=m" (mxcsr)
    );
    return mxcsr;
}

static _always_inline void cpu_fcw_set(uint16_t fcw)
{
    __asm__ __volatile__ (
        "fldcw %w[fcw]\n\t"
        :
        : [fcw] "m" (fcw)
    );
}

static _always_inline uint16_t cpu_fcw_get()
{
    uint16_t fcw;
    __asm__ __volatile__ (
        "fnstcw %w[fcw]\n\t"
        : [fcw] "=m" (fcw)
    );
    return fcw;
}

static _always_inline void cpu_vzeroall_safe()
{
    __asm__ __volatile__ (
        ".pushsection .rodata.fixup.insn\n\t"
        ".quad .Linsn_fixup_%=\n\t"
        ".popsection\n\t"
        ".Linsn_fixup_%=:\n\t"
        "vzeroall\n\t"
        ".byte 0x66, 0x90\n\t"
        : : :
    );
}

static _always_inline void *cpu_stack_ptr_get()
{
    void *rsp;
    __asm__ __volatile__ (
        "movq %%rsp,%[rsp]\n\t"
        : [rsp] "=r" (rsp)
    );
    return rsp;
}

static _always_inline void cpu_crash()
{
    __asm__ __volatile__ (
        "ud2"
    );
}

static _always_inline void cpu_breakpoint()
{
    __asm__ __volatile__ (
        "int3"
    );
}

static _always_inline void cpu_fninit()
{
    __asm__ __volatile__ (
        "fninit\n\t"
    );
}

static _always_inline void cpu_fxsave(void *fpuctx)
{
    __asm__ __volatile__ (
        "fxsave64 (%0)\n\t"
        :
        : "r" (fpuctx)
        : "memory"
    );
}

static _always_inline void cpu_fxrstor(void const *fpuctx)
{
    __asm__ __volatile__ (
        "fxrstor64 (%0)\n\t"
        :
        : "r" (fpuctx)
        : "memory"
    );
}

static _always_inline void cpu_xsave(void *fpuctx)
{
    __asm__ __volatile__ (
        "xsave64 (%[fpuctx])\n\t"
        :
        : "a" (-1), "d" (-1), [fpuctx] "D" (fpuctx)
        : "memory"
    );
}

static _always_inline uint32_t cpu_eflags_get()
{
    uint32_t eflags;
    __asm__ __volatile__ (
        "pushfq\n\t"
        "popq %q[eflags]\n\t"
        : [eflags] "=r" (eflags)
    );
    return eflags;
}

static _always_inline void cpu_eflags_set(uint32_t flags)
{
    __asm__ __volatile__ (
        "push %[flags]\n\t"
        "popf\n\t"
        :
        : [flags] "ir" (flags)
        : "cc"
    );
}

static _always_inline uint32_t cpu_eflags_change(
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

static _always_inline void cpu_stac_safe()
{
    __asm__ __volatile__ (
        ".pushsection .rodata.fixup.insn\n\t"
        ".quad .Linsn_fixup_%=\n\t"
        ".popsection\n\t"
        ".Linsn_fixup_%=:\n\t"
        "stac"
        : : :
    );
}

static _always_inline void cpu_clac_safe()
{
    __asm__ __volatile__ (
        //insn_fixup
        ".pushsection .rodata.fixup.insn\n\t"
        ".quad .Linsn_fixup_%=\n\t"
        ".popsection\n\t"
        ".Linsn_fixup_%=:\n\t"
        "clac"
        : : :
    );
}

static _always_inline void cpu_stac()
{
    __asm__ __volatile__ ("stac");
}

static _always_inline void cpu_clac()
{
    __asm__ __volatile__ ("clac");
}

static _always_inline void cpu_irq_disable()
{
    __asm__ __volatile__ (
        "cli\n\t"
    );
}

static _always_inline
bool cpu_irq_save_disable()
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

static _always_inline _no_instrument
bool cpu_irq_save_disable_noinst()
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

static _always_inline
void cpu_irq_enable()
{
    __asm__ __volatile__ ( "sti" : : : "cc" );
}

_hot
static _always_inline void cpu_irq_toggle(bool enable)
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

_hot _no_instrument
static _always_inline void cpu_irq_toggle_noinst(bool enable)
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

static _always_inline bool cpu_irq_is_enabled()
{
    return cpu_eflags_get() & CPU_EFLAGS_IF;
}

_hot
static _always_inline uint64_t cpu_rdtsc()
{
    uint32_t tsc_lo;
    uint32_t tsc_hi;
    __asm__ __volatile__ (
        "rdtsc\n\t"
        : "=a" (tsc_lo), "=d" (tsc_hi)
    );
    return tsc_lo | ((uint64_t)tsc_hi << 32);
}

_no_instrument
static _always_inline uint64_t cpu_rdtsc_noinstrument()
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
    _always_inline
    cpu_scoped_irq_disable()
        : intr_was_enabled((cpu_irq_save_disable() << 1) - 1)
    {
    }

    _always_inline
    ~cpu_scoped_irq_disable()
    {
        if (intr_was_enabled > 0)
            cpu_irq_toggle(intr_was_enabled > 0);
    }

    _always_inline
    operator bool() const
    {
        return intr_was_enabled > 0;
    }

    _always_inline
    void restore()
    {
        if (intr_was_enabled) {
            cpu_irq_toggle(intr_was_enabled > 0);
            intr_was_enabled = 0;
        }
    }

    _always_inline
    void redisable()
    {
        if (!intr_was_enabled) {
            intr_was_enabled = (cpu_irq_save_disable() << 1) - 1;
        }
    }

private:
    // -1: IRQs were disabled before
    //  0: Don't know anything, haven't done anything
    //  1: IRQs were enabled before
    int8_t intr_was_enabled;
};

class cpu_scoped_wp_disable
{
public:
    _always_inline
    cpu_scoped_wp_disable()
        : wp_was_enabled(init())
    {
    }

    _always_inline
    static int8_t init()
    {
        uintptr_t cr0 = cpu_cr0_get();
        uint8_t result = (cr0 & CPU_CR0_WP_BIT) ? 1 : -1;
        cpu_cr0_set(cr0 & ~CPU_CR0_WP);
        return result;
    }

    _always_inline
    ~cpu_scoped_wp_disable()
    {
        if (wp_was_enabled > 0)
            cpu_cr0_change_bits(0, CPU_CR0_WP);
    }

    _always_inline
    operator bool() const
    {
        return wp_was_enabled > 0;
    }

    _always_inline
    void restore()
    {
        if (wp_was_enabled) {
            cpu_cr0_change_bits(0, CPU_CR0_WP);
            wp_was_enabled = 0;
        }
    }

private:
    int8_t wp_was_enabled;
};

// Monitor/mwait

template<typename T>
static _always_inline void cpu_monitor(
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

static _always_inline void cpu_mwait(uint32_t ext, uint32_t hint)
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
static _always_inline void cpu_wait_masked(
        bool is_equal, T const volatile *value, T wait_value, T mask)
{
    if (cpuid_has_mwait()) {
        while (is_equal != ((atomic_ld_acq(value) & mask) == wait_value)) {
            pause();

            cpu_monitor(value, 0, 0);

            if (is_equal != ((atomic_ld_acq(value) & mask) == wait_value))
                cpu_mwait(0, 0);
            else
                return;
        }
    } else {
        while (is_equal != ((atomic_ld_acq(value) & mask) == wait_value))
            pause();
    }
}

// is_equal: true to wait for value, false to wait for not equal to value
// value: address to watch
// wait_value: comparison value
template<typename T>
static _always_inline void cpu_wait_unmasked(
        bool is_equal, T const volatile *value, T wait_value)
{
    if (cpuid_has_mwait()) {
        while (is_equal != (atomic_ld_acq(value) == wait_value)) {
            pause();

            cpu_monitor(value, 0, 0);

            if (is_equal != (atomic_ld_acq(value) == wait_value))
                cpu_mwait(0, 0);
            else
                break;
        }
    } else {
        while (is_equal != (atomic_ld_acq(value) == wait_value))
            pause();
    }
}

template<typename T>
static _always_inline void cpu_wait_value(
        T const volatile *value, T wait_value)
{
    return cpu_wait_unmasked(true, value, wait_value);
}

template<typename T>
static _always_inline void cpu_wait_not_value(
        T const volatile *value, T wait_value)
{
    return cpu_wait_unmasked(false, value, wait_value);
}


template<typename T>
static _always_inline void cpu_wait_value(
        T const volatile *value, T wait_value, T mask)
{
    return cpu_wait_masked(true, value, wait_value, mask);
}

template<typename T>
static _always_inline void cpu_wait_not_value(
        T const volatile *value, T wait_value, T mask)
{
    return cpu_wait_masked(false, value, wait_value, mask);
}

template<typename T>
static _always_inline void cpu_wait_bit_value(
        T const volatile *value, uint8_t bit, bool bit_value)
{
    return cpu_wait_value(value, T(bit_value) << bit, T(1) << bit);
}

template<typename T>
static _always_inline void cpu_wait_bit_clear(
        T const volatile *value, uint8_t bit)
{
    return cpu_wait_bit_value(value, bit, false);
}

template<typename T>
static _always_inline void cpu_wait_bit_set(
        T const volatile *value, uint8_t bit)
{
    return cpu_wait_bit_value(value, bit, true);
}
