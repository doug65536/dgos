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

#define INTR_EX_BASE        0

#define INTR_EX_DIV         0       // #DE
#define INTR_EX_DEBUG       1       // #DB
#define INTR_EX_NMI         2       // NMI
#define INTR_EX_BREAKPOINT  3       // #BP
#define INTR_EX_OVF         4       // #OF
#define INTR_EX_BOUND       5       // #BR
#define INTR_EX_OPCODE      6       // #UD
#define INTR_EX_DEV_NOT_AV  7       // #NM
#define INTR_EX_DBLFAULT    8       // #DF
#define INTR_EX_COPR_SEG    9       // obsolete, reserved
#define INTR_EX_TSS         10      // #TS
#define INTR_EX_SEGMENT     11      // #NP
#define INTR_EX_STACK       12      // #SS
#define INTR_EX_GPF         13      // #GP
#define INTR_EX_PAGE        14      // #PF
#define INTR_EX_MATH        16      // #MF
#define INTR_EX_ALIGNMENT   17      // #AC
#define INTR_EX_MACHINE     18      // #MC
#define INTR_EX_SIMD        19      // #XF
#define INTR_EX_VIRTUALIZE  20      // (intel)
#define INTR_EX_VMMCOMM     29      // #VC (AMD)
#define INTR_EX_SECURITY    30      // #SX (AMD)

// 21-31 are reserved for future exceptions
#define INTR_EX_LAST        31

// Vectors >= 32 go through generic intr_invoke codepath
#define INTR_SOFT_BASE      32

// Spurious handler must not EOI, so it is outside APIC dispatch range
// Needs to be on a multiple of 8 on some (old) processors
#define INTR_APIC_SPURIOUS  32

// Not really used, but yielded context is written using this interrupt number
#define INTR_THREAD_YIELD   33

// 34-39 reserved

#define INTR_SOFT_LAST      39

// Relatively hot area of the IDT, cache line aligned
// Everything following here needs LAPIC EOI
#define INTR_APIC_DSP_BASE  40
#define INTR_IPI_PANIC      40
#define INTR_APIC_ERROR     41
#define INTR_APIC_THERMAL   42

// Vectors >= 44 go through apic_dispatcher codepath
#define INTR_APIC_TIMER     43
#define INTR_IPI_TLB_SHTDN  44
#define INTR_IPI_RESCHED    45
#define INTR_IPI_FL_TRACE   46
// 47 reserved

// 192 vectors for IOAPIC and MSI(x)
#define INTR_APIC_IRQ_BASE  48
#define INTR_APIC_IRQ_END   239
#define INTR_APIC_DSP_LAST  239
#define INTR_APIC_IRQ_COUNT (INTR_APIC_DSP_LAST - INTR_APIC_IRQ_BASE + 1)
// Everything preceding here needs LAPIC EOI

// Everything following here needs PIC EOI
// Vectors >= 240 go through pic_dispatcher codepath
// PIC IRQs 0xF0-0xFF
#define INTR_PIC_DSP_BASE   240
#define INTR_PIC1_IRQ_BASE  240
#define INTR_PIC1_SPURIOUS  247
#define INTR_PIC2_IRQ_BASE  248
#define INTR_PIC2_SPURIOUS  255
#define INTR_PIC_DSP_LAST   255
// Everything preceding here needs PIC EOI

#define INTR_COUNT 256

#ifndef __ASSEMBLER__

#include "assert.h"

// Spurious vector must be aligned to 8 on really old processors
C_ASSERT((INTR_APIC_SPURIOUS & -8) == INTR_APIC_SPURIOUS);

// The spurious handler must not EOI
C_ASSERT(INTR_APIC_SPURIOUS < INTR_APIC_DSP_BASE ||
         INTR_APIC_SPURIOUS > INTR_APIC_DSP_LAST);

#endif
