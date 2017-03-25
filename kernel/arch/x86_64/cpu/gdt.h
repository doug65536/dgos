#pragma once
#include "types.h"

struct gdt_entry_t {
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
};

struct gdt_entry_tss_ldt_t {
    constexpr gdt_entry_tss_ldt_t(uint64_t base)
        : base_high2((base >> 32) & 0xFFFFFFFF)
        , reserved(0)
    {
    }

    uint32_t base_high2;
    uint32_t reserved;
};

typedef union gdt_entry_combined_t {
    gdt_entry_t mem;
    gdt_entry_tss_ldt_t tss_ldt;

    constexpr gdt_entry_combined_t(gdt_entry_t e) : mem(e) {}
    constexpr gdt_entry_combined_t(gdt_entry_tss_ldt_t e) : tss_ldt(e) {}
} gdt_entry_combined_t;

#define GDT_ACCESS_PRESENT_BIT   7
#define GDT_ACCESS_DPL_BIT       5
#define GDT_ACCESS_EXEC_BIT      3
#define GDT_ACCESS_DOWN_BIT      2
#define GDT_ACCESS_RW_BIT        1

#define GDT_ACCESS_PRESENT      (1 << GDT_ACCESS_PRESENT_BIT)
#define GDT_ACCESS_EXEC         (1 << GDT_ACCESS_EXEC_BIT)
#define GDT_ACCESS_DOWN         (1 << GDT_ACCESS_DOWN_BIT)
#define GDT_ACCESS_RW           (1 << GDT_ACCESS_RW_BIT)

#define GDT_ACCESS_DPL_BITS     2
#define GDT_ACCESS_DPL_MASK     ((1 << GDT_ACCESS_DPL_BITS)-1)
#define GDT_ACCESS_DPL          (GDT_ACCESS_DPL_MASK << GDT_ACCESS_DPL_BIT)
#define GDT_ACCESS_DPL_n(dpl)   ((dpl) << GDT_ACCESS_DPL_BIT)

#define GDT_FLAGS_GRAN_BIT      7
#define GDT_FLAGS_IS32_BIT      6
#define GDT_FLAGS_IS64_BIT      5

#define GDT_FLAGS_GRAN          (1 << GDT_FLAGS_GRAN_BIT)
#define GDT_FLAGS_IS32          (1 << GDT_FLAGS_IS32_BIT)
#define GDT_FLAGS_IS64          (1 << GDT_FLAGS_IS64_BIT)

#define GDT_LIMIT_LOW_MASK      0xFFFF
#define GDT_BASE_LOW_MASK       0xFFFF

#define GDT_BASE_MIDDLE_BIT     16
#define GDT_BASE_MIDDLE         0xFF

#define GDT_LIMIT_HIGH_BIT      16
#define GDT_LIMIT_HIGH          0x0F

#define GDT_BASE_HIGH_BIT       24
#define GDT_BASE_HIGH           0xFF

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

#if 0
#define GDT_MAKE_SEGMENT_DESCRIPTOR( \
    base, limit, present, privilege, \
    type, \
    granularity, is32, is64) \
{ .mem = { \
    ((limit) & GDT_LIMIT_LOW_MASK), \
    ((base) & GDT_BASE_LOW_MASK), \
    (((base) >> GDT_BASE_MIDDLE_BIT) & GDT_BASE_MIDDLE), \
    ( \
        ((present) ? GDT_ACCESS_PRESENT : 0) | \
        GDT_ACCESS_DPL_n(privilege) | \
        (type) \
    ), \
    ( \
        ((granularity) ? GDT_FLAGS_GRAN : 0) | \
        ((is32) ? GDT_FLAGS_IS32 : 0) | \
        ((is64) ? GDT_FLAGS_IS64 : 0) | \
        (((limit) >> GDT_LIMIT_HIGH_BIT) & GDT_LIMIT_HIGH) \
    ), \
    (((base) >> GDT_BASE_HIGH_BIT) & GDT_BASE_HIGH) \
} }
#endif

#define GDT_MAKE_CODEDATA_TYPE(executable, downward, rw) \
    ((1 << 4) | \
    ((executable) ? GDT_ACCESS_EXEC : 0) | \
    ((downward) ? GDT_ACCESS_DOWN : 0) | \
    ((rw) ? GDT_ACCESS_RW : 0))

#define GDT_MAKE_CODEDATA_DESCRIPTOR( \
    base, limit, present, privilege, \
    executable, downward, rw, \
    granularity, is32, is64) \
    GDT_MAKE_SEGMENT_DESCRIPTOR( \
    base, limit, present, privilege, \
    GDT_MAKE_CODEDATA_TYPE(executable, downward, rw), \
    granularity, is32, is64)

#define GDT_TYPE_TSS    0x09

#define GDT_MAKE_TSS_DESCRIPTOR( \
    base, limit, present, privilege, granularity) \
    GDT_MAKE_SEGMENT_DESCRIPTOR( \
    base, limit, present, privilege, \
    GDT_TYPE_TSS, \
    granularity, 0, 0)

#define GDT_MAKE_TSS_HIGH_DESCRIPTOR(base) \
    gdt_entry_combined_t(gdt_entry_tss_ldt_t(base))

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
#define GDT_MAKE_DATASEG64(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0xFFFFF, 1, ring, 0, 0, 1, 1, 0, 0)

#define GDT_MAKE_TSSSEG(base, limit) \
    GDT_MAKE_TSS_DESCRIPTOR(base, limit, 1, 0, 0), \
    GDT_MAKE_TSS_HIGH_DESCRIPTOR(base)

//
// 32-bit descriptors

// Native code (32 bit)
#define GDT_MAKE_CODESEG32(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0xFFFFF, 1, ring, 1, 0, 1, 1, 1, 0)

// Native data (32 bit)
#define GDT_MAKE_DATASEG32(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0xFFFFF, 1, ring, 0, 0, 1, 1, 1, 0)

// Foregn code (16 bit)
#define GDT_MAKE_CODESEG16(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0x0FFFF, 1, ring, 1, 0, 1, 0, 0, 0)

// Foreign data (16 bit)
#define GDT_MAKE_DATASEG16(ring) \
    GDT_MAKE_CODEDATA_DESCRIPTOR(0, 0x0FFFF, 1, ring, 0, 0, 1, 0, 0, 0)

#define GDT_SEL_KERNEL_CODE64   0x08
#define GDT_SEL_KERNEL_DATA64   0x10
#define GDT_SEL_KERNEL_CODE32   0x18
#define GDT_SEL_KERNEL_DATA32   0x20
#define GDT_SEL_KERNEL_CODE16   0x28
#define GDT_SEL_KERNEL_DATA16   0x30
#define GDT_SEL_USER_CODE64     0x38
#define GDT_SEL_USER_DATA64     0x40
#define GDT_SEL_USER_CODE32     0x48
#define GDT_SEL_USER_DATA32     0x50
#define GDT_SEL_TSS             0x60

typedef struct tss_stack_t {
    uint32_t lo;
    uint32_t hi;
} tss_stack_t;

// Task State Segment (64-bit)
typedef struct tss_t {
    uint32_t reserved0;
    tss_stack_t rsp[3];
    tss_stack_t ist[8];
    uint32_t reserved3;
    uint32_t reserved4;
    uint16_t reserved5;
    uint16_t iomap_base;
    // entry 0 is rsp[0], rest are ist stacks
    void *stack[8];
} tss_t;

void gdt_init(void);
void gdt_init_tss(int cpu_count);
void gdt_load_tr(int cpu_number);
