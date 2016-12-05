#pragma once
#include "types.h"

typedef struct gdt_entry_t {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t flags_limit_high;
    uint8_t base_high;
} gdt_entry_t;

typedef struct gdt_entry_tss_ldt_t {
    uint32_t base_high2;
    uint32_t reserved;
} gdt_entry_tss_ldt_t;

typedef union gdt_entry_combined_t {
    gdt_entry_t mem;
    gdt_entry_t tss_ldt;
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
{ .tss_ldt = {  \
    (base) >> 32, 0 \
} }

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

#define GDT_SEL_TSS_n(n)        (GDT_SEL_TSS + ((n) << 4))

// Task State Segment (64-bit)
typedef struct tss_t {
    uint32_t reserved0;
    uint32_t rsp0_lo;
    uint32_t rsp0_hi;
    uint32_t rsp1_lo;
    uint32_t rsp1_hi;
    uint32_t rsp2_lo;
    uint32_t rsp2_hi;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t ist0_lo;
    uint32_t ist0_hi;
    uint32_t ist1_lo;
    uint32_t ist1_hi;
    uint32_t ist2_lo;
    uint32_t ist2_hi;
    uint32_t ist3_lo;
    uint32_t ist3_hi;
    uint32_t ist4_lo;
    uint32_t ist4_hi;
    uint32_t ist5_lo;
    uint32_t ist5_hi;
    uint32_t ist6_lo;
    uint32_t ist6_hi;
    uint32_t ist7_lo;
    uint32_t ist7_hi;
    uint32_t reserved3;
    uint32_t reserved4;
    uint16_t reserved5;
    uint16_t iomap_base;
    void *stack_0;
} tss_t;

void gdt_init(void);
void gdt_init_tss(int cpu_count);
void gdt_load_tr(int cpu_number);
