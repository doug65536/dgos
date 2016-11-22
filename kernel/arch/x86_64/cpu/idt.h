#pragma once

#include "types.h"

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
#define IDT_PRESENT_BIT 7
#define IDT_DPL_BIT     5
#define IDT_TYPE_BIT    0

#define IDT_TYPE_TASK   0x5
#define IDT_TYPE_INTR   0xE
#define IDT_TYPE_TRAP   0xF

#define IDT_PRESENT     (1 << IDT_PRESENT_BIT)

#define IDT_DPL_BITS    2
#define IDT_DPL_MASK    ((1 << IDT_DPL_BITS)-1)
#define IDT_DPL         (IDT_DPL_MASK << IDT_DPL_BIT)

#define IDT_DPL_n(dpl)  (((dpl) & IDT_DPL_MASK) << IDT_DPL_BIT)

#define IDT_TASK        (IDT_TYPE_TASK << IDT_TYPE_BIT)
#define IDT_INTR        (IDT_TYPE_INTR << IDT_TYPE_BIT)
#define IDT_TRAP        (IDT_TYPE_TRAP << IDT_TYPE_BIT)

//
// CPU context

#define EFLAGS_CF_BIT   0
#define EFLAGS_PF_BIT   2
#define EFLAGS_AF_BIT   4
#define EFLAGS_ZF_BIT   6
#define EFLAGS_SF_BIT   7
#define EFLAGS_TF_BIT   8
#define EFLAGS_IF_BIT   9
#define EFLAGS_DF_BIT   10
#define EFLAGS_OF_BIT   11
#define EFLAGS_IOPL_BIT 12
#define EFLAGS_NT_BIT   14
#define EFLAGS_RF_BIT   16
#define EFLAGS_VM_BIT   17
#define EFLAGS_AC_BIT   18
#define EFLAGS_VIF_BIT  19
#define EFLAGS_VIP_BIT  20
#define EFLAGS_ID_BIT   21

#define EFLAGS_CF       (1 << EFLAGS_CF_BIT)
#define EFLAGS_PF       (1 << EFLAGS_PF_BIT)
#define EFLAGS_AF       (1 << EFLAGS_AF_BIT)
#define EFLAGS_ZF       (1 << EFLAGS_ZF_BIT)
#define EFLAGS_SF       (1 << EFLAGS_SF_BIT)
#define EFLAGS_TF       (1 << EFLAGS_TF_BIT)
#define EFLAGS_IF       (1 << EFLAGS_IF_BIT)
#define EFLAGS_DF       (1 << EFLAGS_DF_BIT)
#define EFLAGS_OF       (1 << EFLAGS_OF_BIT)
#define EFLAGS_NT       (1 << EFLAGS_NT_BIT)
#define EFLAGS_RF       (1 << EFLAGS_RF_BIT)
#define EFLAGS_VM       (1 << EFLAGS_VM_BIT)
#define EFLAGS_AC       (1 << EFLAGS_AC_BIT)
#define EFLAGS_VIF      (1 << EFLAGS_VIF_BIT)
#define EFLAGS_VIP      (1 << EFLAGS_VIP_BIT)
#define EFLAGS_ID       (1 << EFLAGS_ID_BIT)

#define EFLAGS_IOPL_BITS    2
#define EFLAGS_IOPL_MASK    ((1 << EFLAGS_IOPL_BITS)-1)
#define EFLAGS_IOPL         (EFLAGS_IOPL_MASK << EFLAGS_IOPL_BIT)

#define MXCSR_IE_BIT        0
#define MXCSR_DE_BIT        1
#define MXCSR_ZE_BIT        2
#define MXCSR_OE_BIT        3
#define MXCSR_UE_BIT        4
#define MXCSR_PE_BIT        5
#define MXCSR_DAZ_BIT       6
#define MXCSR_IM_BIT        7
#define MXCSR_DM_BIT        8
#define MXCSR_ZM_BIT        9
#define MXCSR_OM_BIT        10
#define MXCSR_UM_BIT        11
#define MXCSR_PM_BIT        12
#define MXCSR_RC_BIT        13
#define MXCSR_FZ_BIT        15

#define MXCSR_IE            (1 << MXCSR_IE_BIT)
#define MXCSR_DE            (1 << MXCSR_DE_BIT)
#define MXCSR_ZE            (1 << MXCSR_ZE_BIT)
#define MXCSR_OE            (1 << MXCSR_OE_BIT)
#define MXCSR_UE            (1 << MXCSR_UE_BIT)
#define MXCSR_PE            (1 << MXCSR_PE_BIT)
#define MXCSR_DAZ           (1 << MXCSR_DAZ_BIT)
#define MXCSR_IM            (1 << MXCSR_IM_BIT)
#define MXCSR_DM            (1 << MXCSR_DM_BIT)
#define MXCSR_ZM            (1 << MXCSR_ZM_BIT)
#define MXCSR_OM            (1 << MXCSR_OM_BIT)
#define MXCSR_UM            (1 << MXCSR_UM_BIT)
#define MXCSR_PM            (1 << MXCSR_PM_BIT)
#define MXCSR_FZ            (1 << MXCSR_FZ_BIT)

#define MXCSR_MASK_ALL      (MXCSR_IM | MXCSR_DM | MXCSR_ZM | \
                                MXCSR_OM | MXCSR_UM | MXCSR_PM)

#define MXCSR_RC_NEAREST    0
#define MXCSR_RC_DOWN       1
#define MXCSR_RC_UP         2
#define MXCSR_RC_TRUNCATE   3

#define MXCSR_RC_BITS       2
#define MXCSR_RC_MASK       ((1 << MXCSR_RC_BITS)-1)
#define MXCSR_RC            (MXCSR_RC_MASK << MXCSR_RC_BIT)
#define MXCSR_RC_n(rc)      (((rc) & MXCSR_RC_MASK) << MXCSR_RC_BIT)

typedef struct cpu_flag_info_t {
    char const * const name;
    int bit;
    int mask;
    char const * const *value_names;
} cpu_flag_info_t;

extern cpu_flag_info_t const cpu_eflags_info[];
extern cpu_flag_info_t const cpu_mxcsr_info[];

// Buffer large enough for worst case flags description
#define CPU_MAX_FLAGS_DESCRIPTION    58

size_t cpu_describe_eflags(char *buf, size_t buf_size, uint64_t rflags);
size_t cpu_describe_mxcsr(char *buf, size_t buf_size, uint64_t mxcsr);

size_t cpu_format_flags_register(
        char *buf, size_t buf_size,
        uint64_t rflags, cpu_flag_info_t const *info);

// Passed by ISR handler
typedef struct interrupt_info_t {
    uint64_t interrupt;
    uint64_t error_code;
} interrupt_info_t;

typedef struct isr_ret_frame_t {
    void (*ret_rip)(void);
} isr_ret_frame_t;

typedef struct isr_iret_frame_t {
    int (*rip)(void*);
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} isr_iret_frame_t;

// IRQ handler general registers
typedef struct isr_irq_gpr_t {
    uint16_t s[4];
    void *fsbase;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t rax;
    interrupt_info_t info;
    isr_iret_frame_t iret;
} isr_irq_gpr_t;

// Exception handler context
typedef struct isr_gpr_context_t {
    uint16_t s[4];
    void *fsbase;
    uint64_t r[15];
    interrupt_info_t info;
    isr_iret_frame_t iret;
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

// IRQ handler C call parameter
typedef struct isr_minimal_context_t {
    isr_irq_gpr_t *gpr;
    isr_fxsave_context_t *fpr;
} isr_minimal_context_t;

// Exception handler C call parameter
typedef struct isr_full_context_t {
    isr_gpr_context_t * const gpr;
    isr_fxsave_context_t * const fpr;
} isr_full_context_t;

// Note that fpr must lie on a 16-byte boundary
typedef struct isr_start_context_t {
    isr_minimal_context_t mc;
    isr_fxsave_context_t fpr;
    isr_irq_gpr_t gpr;
    isr_ret_frame_t ret;
} isr_start_context_t;

extern void *(*irq_dispatcher)(
        int irq, isr_minimal_context_t *ctx);

void *isr_handler(isr_minimal_context_t *ctx);

isr_full_context_t *exception_isr_handler(isr_full_context_t *ctx);

int idt_init(void);
