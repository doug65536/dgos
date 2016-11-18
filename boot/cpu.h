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
    ((limit) & 0xFFFF), \
    ((base) & 0xFFFF), \
    (((base) >> 16) & 0xFF), \
    ( \
        ((present) ? 1 << 7 : 0) | \
        (((privilege) & 0x03) << 5) | \
        (1 << 4) | \
        ((executable) ? 1 << 3 : 0) | \
        ((downward) ? 1 << 2 : 0) | \
        ((rw) ? 1 << 1 : 0) \
    ), \
    ( \
        ((granularity) ? 1 << 7 : 0) | \
        ((is32) ? 1 << 6 : 0) | \
        ((is64) ? 1 << 5 : 0) | \
        (((limit) >> 16) & 0x0F) \
    ), \
    (((base) >> 24) & 0xFF) \
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

typedef struct table_register_t {
    uint16_t limit;
    uint16_t base_lo;
    uint16_t base_hi;
} table_register_t;

typedef struct table_register_64_t {
    uint16_t limit;
    uint16_t base_lo;
    uint16_t base_hi;
    uint16_t base_hi1;
    uint16_t base_hi2;
} table_register_64_t;

typedef struct cpuid_t {
    uint32_t eax;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
} cpuid_t;

typedef struct idt_entry_t {
    uint16_t offset_lo; // offset bits 0..15
    uint16_t selector;  // a code segment selector in GDT or LDT
    uint8_t zero;       // unused, set to 0
    uint8_t type_attr;  // type and attributes
    uint16_t offset_hi; // offset bits 16..31
} idt_entry_t;

typedef struct idt_entry_64_t {
    uint16_t offset_lo; // offset bits 0..15
    uint16_t selector;  // a code segment selector in GDT or LDT
    uint8_t zero;       // unused, set to 0
    uint8_t type_attr;  // type and attributes
    uint16_t offset_hi; // offset bits 16..31

    uint32_t offset_64_31;  // offset bits 63..32
    uint32_t reserved;
} idt_entry_64_t;

// idt_entry_t selector field
#define IDT_SEL         0x08
// idt_entry_t type_attr field
#define IDT_PRESENT     0x80
#define IDT_DPL_BIT     5
#define IDT_DPL_BITS    2
#define IDT_DPL_MASK    ((1 << IDT_DPL_BITS)-1)
#define IDT_DPL3        (3 << IDT_DPL_BIT)
#define IDT_TASK        0x05
#define IDT_INTR        0x0E
#define IDT_TRAP        0x0F

uint16_t cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx);

void copy_or_enter(uint64_t address, uint32_t src, uint32_t size);

#define USE_PORT_FUNCTIONS 0
#if USE_PORT_FUNCTIONS
void outb(uint16_t dx, uint8_t al);
void outw(uint16_t dx, uint16_t ax);
void outl(uint16_t dx, uint32_t eax);
uint8_t inb(uint16_t dx);
uint16_t inw(uint16_t dx);
uint32_t inl(uint16_t dx);
#endif

#define USE_8259_PIC_FUNCTIONS 0
#if USE_8259_PIC_FUNCTIONS
void ack_irq(uint8_t irq);
void init_irq(void);
#endif

uint16_t cpu_has_long_mode(void);
uint16_t cpu_has_no_execute(void);
