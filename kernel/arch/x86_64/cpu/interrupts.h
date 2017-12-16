#pragma once

// Interrupts are ordered to simplify dispatch
//  - Exceptions
//  - PIC IRQs
//  - IOAPIC IRQs

// If this is changed, update isr.S

#define INTR_EX_DIV         0x00
#define INTR_EX_DEBUG       0x01
#define INTR_EX_NMI         0x02
#define INTR_EX_BREAKPOINT  0x03
#define INTR_EX_OVF         0x04
#define INTR_EX_BOUND       0x05
#define INTR_EX_OPCODE      0x06
#define INTR_EX_DEV_NOT_AV  0x07
#define INTR_EX_DBLFAULT    0x08
#define INTR_EX_COPR_SEG    0x09
#define INTR_EX_TSS         0x0A
#define INTR_EX_SEGMENT     0x0B
#define INTR_EX_STACK       0x0C
#define INTR_EX_GPF         0x0D
#define INTR_EX_PAGE        0x0E
#define INTR_EX_MATH        0x10
#define INTR_EX_ALIGNMENT   0x11
#define INTR_EX_MACHINE     0x12
#define INTR_EX_SIMD        0x13
#define INTR_EX_VIRTUALIZE  0x14

#define INTR_PIC1_IRQ_BASE  0x20
#define INTR_PIC2_IRQ_BASE  (INTR_PIC1_IRQ_BASE+8)

// IOAPIC vectors followed by dynamically allocated MSI vectors
#define INTR_APIC_IRQ_BASE  0x30
#define INTR_APIC_IRQ_END   0xF0

// Reserved range from 0xF0-0xFA
#define INTR_SOFT_BASE      0xF0

// Software interrupts
#define INTR_THREAD_YIELD   0xFB
#define INTR_APIC_TIMER     0xFC
#define INTR_TLB_SHOOTDOWN  0xFD

#define INTR_APIC_ERROR     0xFE
#define INTR_APIC_SPURIOUS  0xFF
