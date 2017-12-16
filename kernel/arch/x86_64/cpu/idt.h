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
    uint16_t offset_lo;     // offset bits 15:0
    uint16_t selector;      // a code segment selector in GDT
    uint8_t ist;            // interrupt stack table index
    uint8_t type_attr;      // type and attributes
    uint16_t offset_hi;     // offset bits 31:16

    uint32_t offset_64_31;  // offset bits 63:32
    uint32_t reserved;
};

C_ASSERT(sizeof(idt_entry_64_t) == 16);

extern "C" idt_entry_64_t idt[];

// Buffer large enough for worst case flags description
#define CPU_MAX_FLAGS_DESCRIPTION    58

// isr_context->gpr->r order
#define ISR_CTX_REG_RDI(ctx)            ((ctx)->gpr->r[0])
#define ISR_CTX_REG_RSI(ctx)            ((ctx)->gpr->r[1])
#define ISR_CTX_REG_RDX(ctx)            ((ctx)->gpr->r[2])
#define ISR_CTX_REG_RCX(ctx)            ((ctx)->gpr->r[3])
#define ISR_CTX_REG_R8(ctx)             ((ctx)->gpr->r[4])
#define ISR_CTX_REG_R9(ctx)             ((ctx)->gpr->r[5])
#define ISR_CTX_REG_RAX(ctx)            ((ctx)->gpr->r[6])
#define ISR_CTX_REG_RBX(ctx)            ((ctx)->gpr->r[7])
#define ISR_CTX_REG_R10(ctx)            ((ctx)->gpr->r[8])
#define ISR_CTX_REG_R11(ctx)            ((ctx)->gpr->r[9])
#define ISR_CTX_REG_R12(ctx)            ((ctx)->gpr->r[10])
#define ISR_CTX_REG_R13(ctx)            ((ctx)->gpr->r[11])
#define ISR_CTX_REG_R14(ctx)            ((ctx)->gpr->r[12])
#define ISR_CTX_REG_R15(ctx)            ((ctx)->gpr->r[13])
#define ISR_CTX_REG_RBP(ctx)            ((ctx)->gpr->r[14])

#define ISR_CTX_REG_RIP(ctx)            ((ctx)->gpr->iret.rip)
#define ISR_CTX_REG_RSP(ctx)            ((ctx)->gpr->iret.rsp)
#define ISR_CTX_REG_RFLAGS(ctx)         ((ctx)->gpr->iret.rflags)

#define ISR_CTX_REG_CS(ctx)             ((ctx)->gpr->iret.cs)
#define ISR_CTX_REG_SS(ctx)             ((ctx)->gpr->iret.ss)
#define ISR_CTX_REG_DS(ctx)             ((ctx)->gpr->s[0])
#define ISR_CTX_REG_ES(ctx)             ((ctx)->gpr->s[1])
#define ISR_CTX_REG_FS(ctx)             ((ctx)->gpr->s[2])
#define ISR_CTX_REG_GS(ctx)             ((ctx)->gpr->s[3])

#define ISR_CTX_REG_CR3(ctx)            ((ctx)->gpr->cr3)

#define ISR_CTX_ERRCODE(ctx)            ((ctx)->info.error_code)
#define ISR_CTX_INTR(ctx)               ((ctx)->info.interrupt)

#define ISR_CTX_FPU_FCW(ctx)            ((ctx)->fpr->fcw)
#define ISR_CTX_FPU_FOP(ctx)            ((ctx)->fpr->fop)
#define ISR_CTX_FPU_FIP(ctx)            ((ctx)->fpr->fpu_ip)
#define ISR_CTX_FPU_FIS(ctx)            ((ctx)->fpr->cs_or_hi_ip)
#define ISR_CTX_FPU_FDP(ctx)            ((ctx)->fpr->fpu_dp_31_0)
#define ISR_CTX_FPU_FDS(ctx)            ((ctx)->fpr->ds_or_dp_47_32)
#define ISR_CTX_FPU_FSW(ctx)            ((ctx)->fpr->fsw)
#define ISR_CTX_FPU_FTW(ctx)            ((ctx)->fpr->ftw)
#define ISR_CTX_FPU_STn_31_0(ctx, n)    ((ctx)->fpr->st[(n)].st_mm_31_0)
#define ISR_CTX_FPU_STn_63_32(ctx, n)   ((ctx)->fpr->st[(n)].st_mm_63_32)
#define ISR_CTX_FPU_STn_79_64(ctx, n)   ((ctx)->fpr->st[(n)].st_mm_79_64)

#define ISR_CTX_SSE_XMMn_b(ctx, n, i)   ((ctx)->fpr->xmm[(n)].byte[(i)])
#define ISR_CTX_SSE_XMMn_w(ctx, n, i)   ((ctx)->fpr->xmm[(n)].word[(i)])
#define ISR_CTX_SSE_XMMn_d(ctx, n, i)   ((ctx)->fpr->xmm[(n)].dword[(i)])
#define ISR_CTX_SSE_XMMn_q(ctx, n, i)   ((ctx)->fpr->xmm[(n)].qword[(i)])
#define ISR_CTX_SSE_MXCSR(ctx)          ((ctx)->fpr->mxcsr)
#define ISR_CTX_SSE_MXCSR_MASK(ctx)     ((ctx)->fpr->mxcsr_mask)

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
    uint16_t cs_or_hi_ip;
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
        uint16_t st_mm_79_64;
        uint16_t st_mm_reserved_95_80;
        uint32_t st_mm_reserved_127_96;
    } st[8];

    // XMM registers
    union xmm_reg_t {
        uint8_t byte[16];
        uint16_t word[8];
        uint32_t dword[4];
        uint64_t qword[2];
    } xmm[16];
};

// Function to call after switching to another context stack
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

typedef irq_dispatcher_handler_t idt_unhandled_exception_handler_t;

void irq_dispatcher_set_handler(irq_dispatcher_handler_t handler);

// Handle EOI and invoke irq handler
extern "C" isr_context_t *irq_dispatcher(int intr, isr_context_t *ctx);

extern "C" isr_context_t *exception_isr_handler(int intr, isr_context_t *ctx);

extern "C" isr_context_t *isr_handler(int intr, isr_context_t *ctx);

extern "C" void idt_xsave_detect(int ap);

extern "C" isr_context_t *unhandled_exception_handler(isr_context_t *ctx);

void idt_set_unhandled_exception_handler(
        idt_unhandled_exception_handler_t handler);

int idt_init(int ap);

void idt_override_vector(int intr, irq_dispatcher_handler_t handler);
void idt_clone_debug_exception_dispatcher(void);

extern "C" uint32_t xsave_supported_states;
extern "C" uint32_t xsave_enabled_states;
extern "C" void dump_context(isr_context_t *ctx, int to_screen);
