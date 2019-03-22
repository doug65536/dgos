#pragma once

// x86 CPUs implement 16 priority classes.
//
//  CR8      Masks
// -----  -----------
//  0x0     -
//  0x1     -
//  0x2    0x20-0xFF   Highest priority
//  0x3    0xC0-0xFF
//  0x4    0xB0-0xFF
//  0x5    0xA0-0xFF
//  0x6    0x90-0xFF
//  0x7    0x80-0xFF
//  0x8    0x70-0xFF
//  0x9    0x60-0xFF
//  0xA    0x50-0xFF
//  0xB    0x40-0xFF
//  0xC    0x30-0xFF
//  0xD    0x20-0xFF
//  0xE    0x10-0xFF
//  0xF    0x00-0xFF   Lowest priority

// 0x00 - 0x1F (fixed)       - Exceptions
// 0x20 - 0x27 (hi priority) - LAPIC IRQs (error, spurious, timer, etc)
// 0x28 - 0x2F               - reserved for software interrupts
// 0x30 - x-1                - MSI IRQs
//    x - 0xEF               - IOAPIC IRQs
// 0xF0 - 0xFF (lo priority) - PIC IRQs

// If this is changed, update isr.S

#define INTR_EX_DIV         0
#define INTR_EX_DEBUG       1
#define INTR_EX_NMI         2
#define INTR_EX_BREAKPOINT  3
#define INTR_EX_OVF         4
#define INTR_EX_BOUND       5
#define INTR_EX_OPCODE      6
#define INTR_EX_DEV_NOT_AV  7
#define INTR_EX_DBLFAULT    8
#define INTR_EX_COPR_SEG    9
#define INTR_EX_TSS         10
#define INTR_EX_SEGMENT     11
#define INTR_EX_STACK       12
#define INTR_EX_GPF         13
#define INTR_EX_PAGE        14
#define INTR_EX_MATH        16
#define INTR_EX_ALIGNMENT   17
#define INTR_EX_MACHINE     18
#define INTR_EX_SIMD        19
#define INTR_EX_VIRTUALIZE  20

// 21-31 are reserved for future exceptions

// Vectors >= 32 go through generic intr_invoke codepath
#define INTR_SOFT_BASE      32

// Spurious vector must be aligned to 8 on really old processors
#define INTR_APIC_SPURIOUS  32
#define INTR_APIC_ERROR     33
#define INTR_APIC_THERMAL   34
// 35-39 reserved

// Relatively hot area of the IDT, cache line aligned
#define INTR_APIC_TIMER     40
#define INTR_TLB_SHOOTDOWN  41
#define INTR_THREAD_YIELD   42
#define INTR_FLUSH_TRACE    43
#define INTR_IPI_RESCHED    44

// 45-47 reserved

// Vectors >= 48 go through apic_dispatcher codepath
// 192 vectors for IOAPIC and MSI
#define INTR_APIC_IRQ_BASE  48
#define INTR_APIC_IRQ_END   239
#define INTR_APIC_IRQ_COUNT (INTR_APIC_IRQ_END - INTR_APIC_IRQ_BASE + 1)

// Vectors >= 240 go through pic_dispatcher codepath
// PIC IRQs 0xF0-0xFF
#define INTR_PIC1_IRQ_BASE  240
#define INTR_PIC2_IRQ_BASE  248
