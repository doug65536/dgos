#pragma once

// Interrupts are ordered to simplify dispatch
//  - Exceptions
//  - PIC IRQs
//  - IOAPIC IRQs

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

#define INTR_PIC1_IRQ_BASE  0x20
#define INTR_PIC2_IRQ_BASE  (INTR_PIC1_IRQ_BASE+8)

// IOAPIC vectors followed by dynamically allocated MSI vectors
#define INTR_APIC_IRQ_BASE  0x30
#define INTR_APIC_IRQ_END   0xF0

// Reserved range from 0xF0-0xFA
#define INTR_SOFT_BASE      0xF0

#define INTR_APIC_ERROR     0xF1

// Spurious interrupt vector must be aligned to 8 on really old processors
#define INTR_APIC_SPURIOUS  0xF8

// Software interrupts
#define INTR_TLB_SHOOTDOWN  0xFD
#define INTR_THREAD_YIELD   0xFE
#define INTR_APIC_TIMER     0xFF
