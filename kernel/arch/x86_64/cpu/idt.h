#pragma once
#include "types.h"
#include "assert.h"

struct idt_entry_t {
    uint16_t offset_lo; // offset bits 0..15
    uint16_t selector;  // a code segment selector in GDT or LDT
    uint8_t zero;       // unused, set to 0
    uint8_t type_attr;  // type and attributes
    uint16_t offset_hi; // offset bits 16..31
};

C_ASSERT(sizeof(idt_entry_t) == 8);

struct idt_entry_64_t {
    uint16_t offset_lo; // offset bits 0..15
    uint16_t selector;  // a code segment selector in GDT or LDT
    uint8_t ist;        // interrupt stack table index
    uint8_t type_attr;  // type and attributes
    uint16_t offset_hi; // offset bits 16..31

    uint32_t offset_64_31;  // offset bits 63..32
    uint32_t reserved;
};

C_ASSERT(sizeof(idt_entry_64_t) == 16);

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

//
// Floating point control word

#define FPUCW_IM_BIT        0
#define FPUCW_DM_BIT        1
#define FPUCW_ZM_BIT        2
#define FPUCW_OM_BIT        3
#define FPUCW_UM_BIT        4
#define FPUCW_PM_BIT        5
#define FPUCW_PC_BIT        8
#define FPUCW_RC_BIT        10

#define FPUCW_PC_BITS       2
#define FPUCW_RC_BITS       2

#define FPUCW_PC_MASK       ((1U<<FPUCW_PC_BITS)-1)
#define FPUCW_RC_MASK       ((1U<<FPUCW_RC_BITS)-1)

#define FPUCW_IM            (1U<<FPUCW_IM_BIT)
#define FPUCW_DM            (1U<<FPUCW_DM_BIT)
#define FPUCW_ZM            (1U<<FPUCW_ZM_BIT)
#define FPUCW_OM            (1U<<FPUCW_OM_BIT)
#define FPUCW_UM            (1U<<FPUCW_UM_BIT)
#define FPUCW_PM            (1U<<FPUCW_PM_BIT)
#define FPUCW_PC_n(n)       ((n)<<FPUCW_PC_BIT)
#define FPUCW_RC_n(n)       ((n)<<FPUCW_RC_BIT)

#define FPUCW_PC_24         0
#define FPUCW_PC_53         2
#define FPUCW_PC_64         3

#define FPUCW_RC_NEAREST    MXCSR_RC_NEAREST
#define FPUCW_RC_DOWN       MXCSR_RC_DOWN
#define FPUCW_RC_UP         MXCSR_RC_UP
#define FPUCW_RC_TRUNCATE   MXCSR_RC_TRUNCATE

//
// FPU Status Word

#define FPUSW_IE_BIT        0
#define FPUSW_DE_BIT        1
#define FPUSW_ZE_BIT        2
#define FPUSW_OE_BIT        3
#define FPUSW_UE_BIT        4
#define FPUSW_PE_BIT        5
#define FPUSW_SF_BIT        6
#define FPUSW_ES_BIT        7
#define FPUSW_C0_BIT        8
#define FPUSW_C1_BIT        9
#define FPUSW_C2_BIT        10
#define FPUSW_TOP_BIT       11
#define FPUSW_C3_BIT        14
#define FPUSW_B_BIT         15

#define FPUSW_TOP_BITS      2
#define FPUSW_TOP_MASK      ((1U<<FPUSW_TOP_BITS)-1)
#define FPUSW_TOP_n(n)      ((n)<<FPUSW_TOP_BIT)

//
// Exception error code

#define CTX_ERRCODE_PF_P_BIT    0
#define CTX_ERRCODE_PF_W_BIT    1
#define CTX_ERRCODE_PF_U_BIT    2
#define CTX_ERRCODE_PF_R_BIT    3
#define CTX_ERRCODE_PF_I_BIT    4
#define CTX_ERRCODE_PF_PK_BIT   5
#define CTX_ERRCODE_PF_SGX_BIT  15

// Page fault because page not present
#define CTX_ERRCODE_PF_P        (1<<CTX_ERRCODE_PF_P_BIT)

// Page fault was a write
#define CTX_ERRCODE_PF_W        (1<<CTX_ERRCODE_PF_W_BIT)

// Page fault occurred in user mode
#define CTX_ERRCODE_PF_U        (1<<CTX_ERRCODE_PF_U_BIT)

// Page fault because reserved PTE field was 1
#define CTX_ERRCODE_PF_R        (1<<CTX_ERRCODE_PF_R_BIT)

// Page fault was instruction fetch
#define CTX_ERRCODE_PF_I        (1<<CTX_ERRCODE_PF_I_BIT)

// Page fault was protection keys violation
#define CTX_ERRCODE_PF_PK       (1<<CTX_ERRCODE_PF_PK_BIT)

// Page fault was instruction fetch
#define CTX_ERRCODE_PF_SGX      (1<<CTX_ERRCODE_PF_SGX_BIT)

// Buffer large enough for worst case flags description
#define CPU_MAX_FLAGS_DESCRIPTION    58

size_t cpu_describe_eflags(char *buf, size_t buf_size, uintptr_t rflags);
size_t cpu_describe_mxcsr(char *buf, size_t buf_size, uintptr_t mxcsr);
size_t cpu_describe_fpucw(char *buf, size_t buf_size, uint16_t fpucw);
size_t cpu_describe_fpusw(char *buf, size_t buf_size, uint16_t fpusw);

// Passed by ISR handler
struct interrupt_info_t {
    uintptr_t interrupt;
    uintptr_t error_code;
};

struct isr_ret_frame_t {
    void (*ret_rip)(void);
};

struct isr_iret_frame_t {
    int (*rip)(void*);
    uintptr_t cs;
    uintptr_t rflags;
    uintptr_t rsp;
    uintptr_t ss;
};

// Exception handler context
struct isr_gpr_context_t {
    uint16_t s[4];
    uintptr_t cr3;
    void *fsbase;
    uintptr_t r[15];
    interrupt_info_t info;
    isr_iret_frame_t iret;
};

// FPU/SSE context
struct isr_fxsave_context_t {
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
};

// Added to top of context on stack when switching to a context
struct isr_resume_context_t {
    void (*cleanup)(void*);
    void *cleanup_arg;
};

// Exception handler C call parameter
struct isr_context_t {
    isr_resume_context_t resume;
    isr_fxsave_context_t * fpr;
    isr_gpr_context_t * gpr;
};

typedef isr_context_t *(*irq_dispatcher_handler_t)(
        int intr, isr_context_t *ctx);

void irq_dispatcher_set_handler(irq_dispatcher_handler_t handler);

// Handle EOI and invoke irq handler
isr_context_t *irq_dispatcher(int intr, isr_context_t *ctx);

extern "C" void *isr_handler(isr_context_t *ctx);

isr_context_t *exception_isr_handler(isr_context_t *ctx);

extern "C" void idt_xsave_detect(int ap);

int idt_init(int ap);
