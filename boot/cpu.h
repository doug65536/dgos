
#include "types.h"

typedef struct {
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

#define GDT_MAKE_EMPTY() \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

#define GDT_MAKE_CODESEG32(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0xFFFFF, 1, ring, 1, 0, 1, 1, 1, 0)

#define GDT_MAKE_DATASEG32(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0xFFFFF, 1, ring, 0, 0, 1, 1, 1, 0)

#define GDT_MAKE_CODESEG64(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0xFFFFF, 1, ring, 1, 0, 1, 1, 0, 1)

#define GDT_MAKE_DATASEG64(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0xFFFFF, 1, ring, 0, 0, 1, 1, 0, 1)

#define GDT_MAKE_CODESEG16(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0x0FFFF, 1, ring, 1, 0, 1, 0, 0, 0)

#define GDT_MAKE_DATASEG16(ring) \
    GDT_MAKE_SEGMENT_DESCRIPTOR(0, 0x0FFFF, 1, ring, 0, 0, 1, 0, 0, 0)

typedef struct {
    uint16_t limit;
    uint16_t base_lo;
    uint16_t base_hi;
} table_register_t;

typedef struct {
    uint32_t eax;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
} cpuid_t;

typedef struct {
    uint16_t offset_lo; // offset bits 0..15
    uint16_t selector;  // a code segment selector in GDT or LDT
    uint8_t zero;       // unused, set to 0
    uint8_t type_attr;  // type and attributes
    uint16_t offset_hi; // offset bits 16..31
} idt_entry_t;

// idt_entry_t selector field
#define IDT_SEL     0x08
// idt_entry_t type_attr field
#define IDT_PRESENT 0x80
#define IDT_DPL3    0x60
#define IDT_TASK    0x05
#define IDT_INTR    0x0E
#define IDT_TRAP    0x0F

uint16_t cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx);
void copy_to_address(uint64_t address, void *src, uint32_t size);
void outb(uint16_t dx, uint8_t al);
void outw(uint16_t dx, uint16_t ax);
void outl(uint16_t dx, uint32_t eax);
uint8_t inb(uint16_t dx);
uint16_t inw(uint16_t dx);
uint32_t inl(uint16_t dx);
