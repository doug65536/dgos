#pragma once
#include "types.h"
#include "assert.h"
#include "asm_constants.h"
#include "control_regs.h"
#include "control_regs_constants.h"

struct gdt_entry_t {
    constexpr gdt_entry_t()
        : limit_low(0)
        , base_low(0)
        , base_middle(0)
        , access(0)
        , flags_limit_high(0)
        , base_high(0)
    {
    }

    constexpr gdt_entry_t(uint64_t base, uint64_t limit,
                          uint8_t access_, uint8_t flags_)
        : limit_low(limit & 0xFFFF)
        , base_low(base & 0xFFFF)
        , base_middle((base >> 16) & 0xFF)
        , access(access_)
        , flags_limit_high(flags_ | (limit >> 24))
        , base_high(base >> 24)
    {
    }

    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t flags_limit_high;
    uint8_t base_high;

    constexpr gdt_entry_t& set_base(uint32_t base_31_0)
    {
        base_low = base_31_0 & 0xFFFF;
        base_31_0 >>= 16;
        base_middle = base_31_0 & 0xFF;
        base_31_0 >>= 8;
        base_high = base_31_0 & 0xFF;
        return *this;
    }

    constexpr gdt_entry_t& set_limit(uint32_t limit)
    {
        if (limit >= (1U << 20)) {
            limit >>= 12;
            flags_limit_high |= GDT_FLAGS_GRAN;
        } else {
            flags_limit_high &= ~GDT_FLAGS_GRAN;
        }
        limit_low = limit & 0xFFFF;
        limit >>= 16;
        flags_limit_high &= 0xF0;
        limit &= 0xF;
        flags_limit_high = (flags_limit_high & 0xF0) | limit;
        return *this;
    }

    constexpr gdt_entry_t& set_type(uint8_t type)
    {
        access = (access & ~0xF) | type;
        return *this;
    }

    constexpr gdt_entry_t& set_present(bool present)
    {
        access = (access & ~GDT_ACCESS_PRESENT) |
                (present ? GDT_ACCESS_PRESENT : 0);
        return *this;
    }

    constexpr gdt_entry_t& set_flags(uint8_t flags)
    {
        flags_limit_high &= 0x0F;
        flags &= 0xF0;
        flags_limit_high |= flags;
        return *this;
    }

    constexpr uint8_t get_type()
    {
        return access & 0xF;
    }
};

C_ASSERT(sizeof(gdt_entry_t) == 8);

struct gdt_entry_tss_ldt_t {
    constexpr gdt_entry_tss_ldt_t()
        : base_high2(0)
        , reserved(0)
    {
    }

    constexpr gdt_entry_tss_ldt_t(uint32_t high)
        : base_high2(high)
        , reserved(0)
    {
    }

    constexpr gdt_entry_tss_ldt_t& set_base_hi(uint32_t high)
    {
        base_high2 = high;
        return *this;
    }

    uint32_t base_high2;
    uint32_t reserved;
};

union gdt_entry_combined_t {
    gdt_entry_t mem;
    gdt_entry_tss_ldt_t tss_ldt;
    uint64_t raw;

    constexpr gdt_entry_combined_t(gdt_entry_t e) : mem(e) {}
    constexpr gdt_entry_combined_t(gdt_entry_tss_ldt_t e) : tss_ldt(e) {}
};

C_ASSERT(sizeof(gdt_entry_t) == 8);
C_ASSERT(sizeof(gdt_entry_tss_ldt_t) == 8);
C_ASSERT(sizeof(gdt_entry_combined_t) == 8);

#define GDT_MAKE_SEGMENT_DESCRIPTOR( \
    base, limit, present, privilege, \
    type, \
    granularity, is32, is64) \
    gdt_entry_combined_t(gdt_entry_t(base, limit, \
        ((present) ? GDT_ACCESS_PRESENT : 0) | \
        GDT_ACCESS_DPL_n(privilege) | \
        (type), \
        ((granularity) ? GDT_FLAGS_GRAN : 0) | \
        ((is32) ? GDT_FLAGS_IS32 : 0) | \
        ((is64) ? GDT_FLAGS_IS64 : 0) | \
        (((limit) >> GDT_LIMIT_HIGH_BIT) & GDT_LIMIT_HIGH)))

#define GDT_MAKE_CODEDATA_TYPE(executable, downward, rw) \
    ((1 << 4) | \
    ((executable) ? GDT_ACCESS_EXEC : 0) | \
    ((downward) ? GDT_ACCESS_DOWN : 0) | \
    ((rw) ? GDT_ACCESS_RW : 0)) | \
    GDT_ACCESS_ACCESSED

#define GDT_MAKE_CODEDATA_DESCRIPTOR( \
    base, limit, present, privilege, \
    executable, downward, rw, \
    granularity, is32, is64) \
    GDT_MAKE_SEGMENT_DESCRIPTOR( \
    base, limit, present, privilege, \
    GDT_MAKE_CODEDATA_TYPE(executable, downward, rw), \
    granularity, is32, is64)

#define GDT_MAKE_TSS_DESCRIPTOR( \
    base_lo, limit, present, privilege, granularity) \
    GDT_MAKE_SEGMENT_DESCRIPTOR( \
    base_lo, limit, present, privilege, \
    GDT_TYPE_TSS, \
    granularity, 0, 0)

#define GDT_MAKE_TSS_HIGH_DESCRIPTOR(base_hi) \
    gdt_entry_combined_t(gdt_entry_tss_ldt_t(base_hi))

#if 0
#define GDT_MAKE_TSS_HIGH_DESCRIPTOR(base) \
{ .tss_ldt = {  \
    (base) >> 32, 0 \
} }
#endif

//
// Generic descriptors

#define GDT_MAKE_EMPTY() \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0, 0, 0, 0, 0, 0, 0)

//
// 64-bit descriptors

// Native code (64 bit)
#define GDT_MAKE_CODESEG64(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0xFFFFF, 1, ring, 1, 0, 1, 1, 0, 1)

// Native data (64 bit)
// 3.4.5 "When not in IA-32e mode or for non-code segments, bit 21 is
// reserved and should always be set to 0"
#define GDT_MAKE_DATASEG(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0xFFFFF, 1, ring, 0, 0, 1, 1, 1, 0)

#define GDT_MAKE_TSSSEG(base_lo, base_hi, limit) \
    GDT_MAKE_TSS_DESCRIPTOR(base_lo, limit, 1, 0, 0), \
    GDT_MAKE_TSS_HIGH_DESCRIPTOR(base_hi)

//
// 32-bit descriptors

// Native code (32 bit)
#define GDT_MAKE_CODESEG32(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0xFFFFF, 1, ring, 1, 0, 1, 1, 1, 0)

// 32 bit data same as 64 bit data

// Foreign code (16 bit)
#define GDT_MAKE_CODESEG16(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0x0FFFF, 1, ring, 1, 0, 1, 0, 0, 0)

// Foreign data (16 bit)
#define GDT_MAKE_DATASEG16(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0x0FFFF, 1, ring, 0, 0, 1, 0, 0, 0)

// Task State Segment (64-bit)
struct tss_t {
    // EVERYTHING is misaligned without dummy_align
    uint32_t dummy_align;

    // Beginning of TSS is actually here
    uint32_t reserved0;

    uint64_t rsp[3];

    uint64_t ist[8];

    uint32_t reserved3;
    uint32_t reserved4;

    uint16_t reserved5;
    uint16_t iomap_base;
    uint32_t dummy_align2;

    // entry 0 is rsp[0], rest are ist stacks
    void *stack[8];

    uint8_t pad_to_256[256 - 176];
};

// Ensure no false sharing
C_ASSERT((sizeof(tss_t) & 63) == 0);

// Ensure no spanning page boundaries
C_ASSERT(4096 % sizeof(tss_t) == 0);

C_ASSERT(offsetof(tss_t, rsp) == TSS_RSP0_OFS);

extern tss_t tss_list[];

void gdt_init(int ap);
void gdt_init_tss(size_t cpu_count);
void gdt_load_tr(int cpu_number);

extern "C" gdt_entry_combined_t gdt[];
extern "C" void gdt_init_tss_early();

extern "C" table_register_64_t gdtr;
