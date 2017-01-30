#pragma once

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

#define INTR_THREAD_YIELD   72
#define INTR_APIC_TIMER     73
#define INTR_TLB_SHOOTDOWN  74

// IOAPIC interrupt vectors are allocated
// below 0xFF. Variable count depending upon
// the number of IOAPICs and their redirection
// table size

#define INTR_APIC_SPURIOUS  0xFF
