#pragma once

#include "types.h"
#include "cpuid.h"
#include "cpu_constants.h"
#include "assert.h"

struct gdt_entry_t {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t flags_limit_high;
    uint8_t base_high;

    gdt_entry_t &set_base(uint32_t base)
    {
        base_low = base & GDT_BASE_LOW_MASK;
        base_middle = (base >> GDT_BASE_MIDDLE_BIT) & 0xFF;
        base_high = base >> GDT_BASE_HIGH_BIT;
        return *this;
    }

    gdt_entry_t &set_limit(uint32_t limit)
    {
        if (limit > 0xFFFFFF) {
            // Translate limit to page count and set granularity bit
            limit >>= 12;
            flags_limit_high |= GDT_FLAGS_GRAN;
        } else {
            // Clear granularity bit
            flags_limit_high &= ~GDT_FLAGS_GRAN;
        }

        limit_low = limit & GDT_BASE_LOW_MASK;
        flags_limit_high = (flags_limit_high & ~GDT_LIMIT_HIGH_MASK) |
                ((limit >> GDT_LIMIT_HIGH_BIT) & GDT_LIMIT_HIGH_MASK);
        return *this;
    }

    gdt_entry_t &set_access(bool present, int priv,
                            bool exec, bool down, bool rw)
    {
        access = (present ? GDT_ACCESS_PRESENT : 0) |
                GDT_ACCESS_DPL_n(priv) |
                (1 << 4) |
                (exec ? GDT_ACCESS_EXEC : 0) |
                (down ? GDT_ACCESS_DOWN : 0) |
                (rw ? GDT_ACCESS_RW : 0) |
                GDT_ACCESS_ACCESSED;
        return *this;
    }

    gdt_entry_t &set_flags(bool is32, bool is64)
    {
        flags_limit_high = (flags_limit_high & GDT_FLAGS_GRAN) |
                (is32 ? GDT_FLAGS_IS32 : 0) |
                (is64 ? GDT_FLAGS_IS64 : 0) |
                (flags_limit_high & GDT_LIMIT_HIGH_MASK);
        return *this;
    }
};

C_ASSERT(sizeof(gdt_entry_t) == 8);

extern gdt_entry_t gdt[];

#define GDT_MAKE_SEGMENT_DESCRIPTOR(base, \
            limit, \
            present, \
            privilege, \
            executable, \
            downward, \
            rw, \
            granularity, \
            is32, \
            is64) \
{ \
    ((limit) & GDT_LIMIT_LOW_MASK), \
    ((base) & GDT_BASE_LOW_MASK), \
    (((base) >> GDT_BASE_MIDDLE_BIT) & GDT_BASE_MIDDLE), \
    ( \
        ((present) ? GDT_ACCESS_PRESENT : 0) | \
        GDT_ACCESS_DPL_n(privilege) | \
        (1 << 4) | \
        ((executable) ? GDT_ACCESS_EXEC : 0) | \
        ((downward) ? GDT_ACCESS_DOWN : 0) | \
        ((rw) ? GDT_ACCESS_RW : 0) | \
        (GDT_ACCESS_ACCESSED) \
    ), \
    ( \
        ((granularity) ? GDT_FLAGS_GRAN : 0) | \
        ((is32) ? GDT_FLAGS_IS32 : 0) | \
        ((is64) ? GDT_FLAGS_IS64 : 0) | \
        (((limit) >> GDT_LIMIT_HIGH_BIT) & GDT_LIMIT_HIGH_MASK) \
    ), \
    (((base) >> GDT_BASE_HIGH_BIT) & GDT_BASE_HIGH) \
}

//
// 32-bit selectors

#define GDT_MAKE_EMPTY() \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

// Native code (32 bit)
#define GDT_MAKE_CODESEG32(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0xFFFFF, 1, ring, 1, 0, 1, 1, 1, 0)

// Native data (32 bit)
#define GDT_MAKE_DATASEG32(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0xFFFFF, 1, ring, 0, 0, 1, 1, 1, 0)

// Foregn code (16 bit)
#define GDT_MAKE_CODESEG16(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0x0FFFF, 1, ring, 1, 0, 1, 0, 0, 0)

// Foreign data (16 bit)
#define GDT_MAKE_DATASEG16(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0x0FFFF, 1, ring, 0, 0, 1, 0, 0, 0)

//
// 64-bit selectors

// Native code (64 bit)
#define GDT_MAKE_CODESEG64(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0xFFFFF, 1, ring, 1, 0, 1, 1, 0, 1)

// Native data (64 bit)
// 3.4.5 "When not in IA-32e mode or for non-code segments, bit 21 is
// reserved and should always be set to 0"
#define GDT_MAKE_DATASEG64(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0xFFFFF, 1, ring, 0, 0, 1, 1, 0, 0)

struct table_register_t {
    uint16_t align;
    uint16_t limit;
    uint32_t base;
};

struct table_register_64_t {
    uint16_t align[3];
    uint16_t limit;
    uint32_t base_lo;
    uint32_t base_hi;
};

struct idt_entry_t {
    uint16_t offset_lo; // offset bits 0..15
    uint16_t selector;  // a code segment selector in GDT or LDT
    uint8_t zero;       // unused, set to 0
    uint8_t type_attr;  // type and attributes
    uint16_t offset_hi; // offset bits 16..31
};

struct idt_entry_64_t {
    uint16_t offset_lo; // offset bits 0..15
    uint16_t selector;  // a code segment selector in GDT or LDT
    uint8_t zero;       // unused, set to 0
    uint8_t type_attr;  // type and attributes
    uint16_t offset_hi; // offset bits 16..31

    uint32_t offset_64_31;  // offset bits 63..32
    uint32_t reserved;
};

extern "C" void cpu_a20_init();
extern "C" void cpu_a20_enterpm();
extern "C" void cpu_a20_exitpm();
extern "C" bool cpu_a20_toggle(bool enabled);
extern "C" void cpu_a20_wait(bool enabled);

static _always_inline void outb(uint16_t dx, uint8_t al)
{
    __asm__ __volatile__ (
        "outb %b[val],%w[port]"
        :
        : [val] "a" (al)
        , [port] "Nd" (dx));
}

static _always_inline void outw(uint16_t dx, uint16_t ax)
{
    __asm__ __volatile__ (
        "outw %w[val],%w[port]"
        :
        : [val] "a" (ax)
        , [port] "Nd" (dx));
}

static _always_inline void outl(uint16_t dx, uint32_t eax)
{
    __asm__ __volatile__ (
        "outl %[val],%w[port]"
        :
        : [val] "a" (eax)
        , [port] "Nd" (dx));
}

static _always_inline void outsb(uint16_t dx, void const *src, size_t n)
{
    __asm__ __volatile__ (
        "rep outsb"
        : [src] "+S" (src)
        , [n] "+c" (n)
        : [port] "d" (dx)
        : "memory");
}

static _always_inline void outsw(uint16_t dx, void const *src, size_t n)
{
    __asm__ __volatile__ (
        "rep outsw"
        : [src] "+S" (src)
        , [n] "+c" (n)
        : [port] "d" (dx)
        : "memory");
}

static _always_inline void outsl(uint16_t dx, void const *src, size_t n)
{
    __asm__ __volatile__ (
        "rep outsl"
        : [src] "+S" (src)
        , [n] "+c" (n)
        : [port] "d" (dx)
        : "memory");
}

static _always_inline uint8_t inb(uint16_t dx)
{
    uint8_t al;
    __asm__ __volatile__ (
        "inb %w[port],%b[val]"
        : [val] "=a" (al)
        : [port] "Nd" (dx));
    return al;
}

static _always_inline uint16_t inw(uint16_t dx)
{
    uint16_t ax;
    __asm__ __volatile__ (
        "inw %w[port],%w[val]"
        : [val] "=a" (ax)
        : [port] "Nd" (dx));
    return ax;
}

static _always_inline uint32_t inl(uint16_t dx)
{
    uint32_t eax;
    __asm__ __volatile__ (
        "inl %w[port],%[val]"
        : [val] "=a" (eax)
        : [port] "Nd" (dx));
    return eax;
}

static _always_inline void insb(uint16_t dx, void *dst, size_t n)
{
    __asm__ __volatile__ (
        "rep insb"
        : [dst] "+D" (dst)
        , [n] "+c" (n)
        : [port] "d" (dx)
        : "memory");
}

static _always_inline void insw(uint16_t dx, void *dst, size_t n)
{
    __asm__ __volatile__ (
        "rep insw"
        : [dst] "+D" (dst)
        , [n] "+c" (n)
        : [port] "d" (dx)
        : "memory");
}

static _always_inline void insl(uint16_t dx, void *dst, size_t n)
{
    __asm__ __volatile__ (
        "rep insl"
        : [dst] "+D" (dst)
        , [n] "+c" (n)
        : [port] "d" (dx)
        : "memory");
}

static _always_inline void pause()
{
    __asm__ __volatile__ ("pause");
}

static _always_inline void cpu_flush_cache()
{
    __asm__ __volatile__ ("wbinvd\n\t");
}

static _always_inline uint64_t cpu_rdtsc()
{
    uint32_t hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=d" (hi), "=a" (lo));
    return (uint64_t(hi) << 32) | lo;
}

static _always_inline uint64_t cpu_rdrand()
{
    uint32_t hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=d" (hi), "=a" (lo));
    return (uint64_t(hi) << 32) | lo;
}

static _always_inline uint64_t rol64(uint64_t n, uint8_t bits)
{
    return (n << bits) | (n >> (64 - bits));
}

#define USE_8259_PIC_FUNCTIONS 0
#if USE_8259_PIC_FUNCTIONS
void ack_irq(uint8_t irq);
void init_irq();
#endif

extern "C" void cpu_init();

_noreturn
void run_kernel(uint64_t entry, void *param);
void copy_kernel(uint64_t dest_addr, void *src, size_t sz);
void reloc_kernel(uint64_t distance, void *elf_rela, size_t relcnt);

extern "C" uint8_t xcr0_value;

extern bool nx_available;
extern uint32_t gp_available;
