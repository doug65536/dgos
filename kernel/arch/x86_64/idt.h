#pragma once

#include "types.h"

typedef struct {
    uint16_t offset_lo; // offset bits 0..15
    uint16_t selector;  // a code segment selector in GDT or LDT
    uint8_t zero;       // unused, set to 0
    uint8_t type_attr;  // type and attributes
    uint16_t offset_hi; // offset bits 16..31
} idt_entry_t;

typedef struct {
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

// Passed by ISR handler
typedef struct interrupt_info_t {
    uint64_t interrupt;
    uint64_t error_code;
} interrupt_info_t;

// General register context
typedef struct isr_gpr_context_t {
    uint64_t r[16];
    interrupt_info_t info;
    void *rip;
    uint64_t cs;
    uint64_t rflags;
} isr_gpr_context_t;

// FPU/SSE context
typedef struct isr_fxsave_context_t {
    // FPU control word
    uint16_t fcw;
    // FPU status word
    uint16_t fsw;
    // FPU tag word (bitmap of non-empty mm/st regs)
    uint8_t ftw;
    uint8_t reserved_1;
    // FPU opcode
    uint16_t fop;
    // FPU IP
    uint32_t fpu_ip;
    uint16_t hi_ip_or_cs;
    uint16_t reserved_2;

    // FPU data pointer
    uint32_t fpu_dp_31_0;
    uint16_t ds_or_dp_47_32;
    uint16_t reserved_3;
    // SSE status register
    uint32_t mxcsr;
    uint32_t mxcsr_mask;

    // FPU/MMX registers
    struct fpu_reg_t {
        uint32_t st_mm_31_0;
        uint32_t st_mm_63_32;
        uint16_t st_mm_79_63;
        uint16_t st_mm_reserved_95_80;
        uint32_t st_mm_reserved_127_96;
    } fpu_regs[8];

    // XMM registers
    union xmm_reg_t {
        uint8_t byte[16];
        uint16_t word[8];
        uint32_t dword[4];
        uint64_t qword[2];
    } xmm[16];
} isr_fxsave_context_t;

typedef struct isr_full_context_t {
    isr_gpr_context_t * const gpr;
    isr_fxsave_context_t * const fpr;
} isr_full_context_t;

extern void *(*irq_dispatcher)(int irq, void *stack_pointer);

void *isr_handler(interrupt_info_t *info, void *stack_pointer);

isr_full_context_t *exception_isr_handler(isr_full_context_t *ctx);

int idt_init(void);
