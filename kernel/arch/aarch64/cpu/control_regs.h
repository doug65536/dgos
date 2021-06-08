#pragma once

#include "types.h"
//#include "cpu/cpuid.h"
#include "atomic.h"
#include "thread.h"
#include "cpu/control_regs_constants.h"

static _always_inline void cpu_halt()
{
    __asm__ __volatile__ (
        "wfi"
    );
}

static _always_inline void cpu_flush_cache()
{
    // TODO
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
    // todo
//    constexpr uintptr_t enable_mask = ~CPU_DR7_BPn_MASK(dr);
//    uintptr_t enable_value = CPU_DR7_BPn_VAL(dr, enable, rw, len);

//    uintptr_t ctl = cpu_debug_reg_get<7>();

//    // Disable it before changing address
//    ctl &= enable_mask;
//    cpu_debug_reg_set<7>(ctl);
//    cpu_debug_reg_set<dr>(addr);

//    // Enable it
//    ctl |= enable_value;
//    cpu_debug_reg_set<7>(ctl);
}

void cpu_debug_breakpoint_set_indirect(uintptr_t addr, int rw,
                                       int len, int enable, size_t index);

template<uint8_t el>
static _always_inline void cpu_page_directory_set(uintptr_t ttbr)
{
    __asm__ __volatile__ (
        "msr ttbr%[level]_el1,%[value]\n\t"
        :
        : [el0_root] "r" (ttbr)
        , [level] "n" (el)
        : "memory"
    );
}

template<uint8_t el>
static _always_inline uintptr_t cpu_page_directory_get()
{
    uintptr_t ttbr;
    __asm__ __volatile__ (
        "mrs %[value],ttbr%[level]_el0\n\t"
        : [value] "=r" (ttbr)
        : [level] "n" (el)
    );
    return ttbr;
}

static _always_inline uintptr_t cpu_fault_address_get()
{
    uintptr_t addr;
    __asm__ __volatile__ (
        "mrs %[addr],far_el1\n\t"
        : [addr] "=r" (addr)
    );
    return addr;
}

static _always_inline void cpu_fault_address_set(uintptr_t addr)
{
    __asm__ __volatile__ (
        "msr far_el1,%[addr]\n\t"
        :
        : [addr] "r" (addr)
    );
}

static _always_inline void cpu_page_invalidate(uintptr_t addr)
{
    __asm__ __volatile__ (
        "tlbi  %[addr]\n\t"
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
    __asm__ __volatile__ (
        "tlbi vmalle1is"
        :
        :
        : "memory"
    );
}

static _always_inline void cpu_cache_flush()
{
    __asm__ __volatile__ ("wbinvd\n\t" : : : "memory");
}

static _always_inline void *cpu_stack_ptr_get()
{
    void *rsp;
    __asm__ __volatile__ (
        "mov %[rsp],sp\n\t"
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

static _always_inline uintptr_t cpu_cpsr_get()
{
    uintptr_t value;
    __asm__ __volatile__ (
        "mrs %[value],cpsr"
        : [value] "=r" (value)
    );
    return value;
}

static _always_inline void cpu_cpsr_set(uintptr_t value)
{
    __asm__ __volatile__ (
        "msr cpsr,%[value]"
        :
        : [value] "r" (value)
    );
}

static _always_inline void cpu_irq_disable()
{
    __asm__ __volatile__ (
        "msr daifset,#2\n\t"
    );
}

static _always_inline uintptr_t cpu_daif_get()
{
    uintptr_t daif;
    __asm__ __volatile__ (
        "mrs %[value],daif\n\t"
        : [value] "=r" (daif)
    );
    return daif;
}

static _always_inline _no_instrument uintptr_t cpu_daif_get_noinst()
{
    uintptr_t daif;
    __asm__ __volatile__ (
        "mrs %[value],daif\n\t"
        : [value] "=r" (daif)
    );
    return daif;
}

static _always_inline void cpu_daif_set(uintptr_t daif)
{
    __asm__ __volatile__ (
        "msr daif,%[value]\n\t"
        :
        : [value] "r" (daif)
    );
}

static _always_inline void cpu_daif_set_noinst(uintptr_t daif)
{
    __asm__ __volatile__ (
        "msr daif,%[value]\n\t"
        :
        : [value] "r" (daif)
    );
}

template<uint8_t bits>
static _always_inline void cpu_daifset_write()
{
    __asm__ __volatile__ (
        "msr daifset,%[value]\n\t"
        :
        : [value] "i" (bits >> CPU_DAIF_DAIF_BIT)
    );
}

template<uint8_t bits>
static _always_inline void cpu_daifclr_write()
{
    __asm__ __volatile__ (
        "msr daifclr,%[value]\n\t"
        :
        : [value] "i" (bits >> CPU_DAIF_DAIF_BIT)
    );
}

template<uint8_t bits>
static _always_inline _no_instrument void cpu_daifclr_write_noinst()
{
    __asm__ __volatile__ (
        "msr daifclr,%[value]\n\t"
        :
        : [value] "i" (bits >> CPU_DAIF_DAIF_BIT)
    );
}

static _always_inline
uint8_t cpu_irq_save_disable_all()
{
    uintptr_t daif = cpu_daif_get();
    cpu_daifset_write<0xF>();
    return uint8_t(daif >> CPU_DAIF_DAIF_BIT);
}

static _always_inline
uint8_t cpu_irq_save_disable()
{
    uintptr_t daif = cpu_daif_get();
    cpu_daifset_write<CPU_DAIF_I>();
    return uint8_t(daif >> CPU_DAIF_DAIF_BIT);
}

static _always_inline _no_instrument
uint8_t cpu_irq_save_disable_noinst()
{
    uintptr_t daif = cpu_daif_get_noinst();
    cpu_daifclr_write_noinst<CPU_DAIF_I>();
    return uint8_t(daif >> CPU_DAIF_DAIF_BIT);
}

static _always_inline
void cpu_irq_enable()
{
    __asm__ __volatile__ (
        "msr daifclr,0xF\n\t"
    );
}

_hot
static _always_inline void cpu_irq_toggle(uint8_t enable)
{
    uintptr_t temp;
    __asm__ __volatile__ (
        "mrs %[temp],daif\n\t"
        "bic %[temp],%[temp],%[mask_daif]\n\t"
        "orr %[temp],%[temp],%[enable]\n\t"
        "msr daif,%[temp]\n\t"
        : [temp] "=&r" (temp)
        : [enable] "r" (uintptr_t(enable))
        , [mask_daif] "i" (uintptr_t(CPU_DAIF))
        : "cc"
    );
    (void)temp;
}

_hot _no_instrument
static _always_inline void cpu_irq_toggle_noinst(bool enable)
{
    uintptr_t temp;
    __asm__ __volatile__ (
        "mrs %[temp],daif\n\t"
        "bic %[temp],%[temp],%[mask_daif]\n\t"
        "orr %[temp],%[temp],%[enable]\n\t"
        "msr daif,%[temp]\n\t"
        : [temp] "=&r" (temp)
        : [enable] "r" (uintptr_t(enable))
        , [mask_daif] "i" (uintptr_t(CPU_DAIF))
        : "cc"
    );
    (void)temp;
}

static _always_inline bool cpu_irq_is_enabled()
{
    return cpu_daif_get() & CPU_DAIF_I;
}

static _always_inline void cpu_clzero(void *addr)
{
    __asm__ __volatile__ (
        "clzero %[operand]"
        :
        : [operand] "m" (*(char*)addr)
        : "memory"
    );
}

static _always_inline void cpu_clflush(void *addr)
{
    __asm__ __volatile__ (
        "dc zva,%[operand]"
        : [operand] "+m" (*(char*)addr)
        :
        : "memory"
    );
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
        if (!intr_was_enabled)
            intr_was_enabled = (cpu_irq_save_disable() << 1) - 1;
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
        //: wp_was_enabled(init())
    {
    }

    _always_inline
    static int8_t init()
    {
//        uintptr_t cr0 = cpu_cr0_get();
//        uint8_t result = (cr0 & CPU_CR0_WP_BIT) ? 1 : -1;
//        cpu_cr0_set(cr0 & ~CPU_CR0_WP);
//        return result;
        return 0;
    }

    _always_inline
    ~cpu_scoped_wp_disable()
    {
//        if (wp_was_enabled > 0)
//            cpu_cr0_change_bits(0, CPU_CR0_WP);
    }

    _always_inline
    operator bool() const
    {
//        return wp_was_enabled > 0;
        return false;
    }

    _always_inline
    void restore()
    {
//        if (wp_was_enabled) {
//            cpu_cr0_change_bits(0, CPU_CR0_WP);
//            wp_was_enabled = 0;
//        }
    }

private:
//    int8_t wp_was_enabled;
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
    while (is_equal != ((atomic_ld_acq(value) & mask) == wait_value)) {
        if (spincount_mask == 0)
            thread_yield();
        else
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
    while (is_equal != (atomic_ld_acq(value) == wait_value))
        pause();
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

static _always_inline void cpu_mmio_wr(
        void const volatile *mmio, uint8_t value)
{
    __asm__ __volatile__ (
        "movb %b[value],(%[mmio])\n"
        :
        : [mmio] "a" (mmio)
        , [value] "ri" (value)
    );
}

static _always_inline void cpu_mmio_wr(
        void const volatile *mmio, uint16_t value)
{
    __asm__ __volatile__ (
        "movw %w[value],(%[mmio])\n"
        :
        : [mmio] "a" (mmio)
        , [value] "ri" (value)
    );
}

static _always_inline void cpu_mmio_wr(
        void const volatile *mmio, uint32_t value)
{
    __asm__ __volatile__ (
        "movl %k[value],(%[mmio])\n"
        :
        : [mmio] "a" (mmio)
        , [value] "ri" (value)
    );
}

static _always_inline void cpu_mmio_wr(
        void const volatile *mmio, uint64_t value)
{
    __asm__ __volatile__ (
        "movq %q[value],(%[mmio])\n"
        :
        : [mmio] "a" (mmio)
        , [value] "r" (value)
    );
}

static _always_inline _no_instrument void cpu_mmio_wr_noinst(
        void const volatile *mmio, uint8_t value)
{
    __asm__ __volatile__ (
        "movb %b[value],(%[mmio])\n"
        :
        : [mmio] "a" (mmio)
        , [value] "ri" (value)
    );
}

static _always_inline _no_instrument void cpu_mmio_wr_noinst(
        void const volatile *mmio, uint16_t value)
{
    __asm__ __volatile__ (
        "movw %w[value],(%[mmio])\n"
        :
        : [mmio] "a" (mmio)
        , [value] "ri" (value)
    );
}

static _always_inline _no_instrument void cpu_mmio_wr_noinst(
        void const volatile *mmio, uint32_t value)
{
    __asm__ __volatile__ (
        "movl %k[value],(%[mmio])\n"
        :
        : [mmio] "a" (mmio)
        , [value] "ri" (value)
    );
}

static _always_inline _no_instrument void cpu_mmio_wr_noinst(
        void const volatile *mmio, uint64_t value)
{
    __asm__ __volatile__ (
        "movq %q[value],(%[mmio])\n"
        :
        : [mmio] "a" (mmio)
        , [value] "r" (value)
    );
}
