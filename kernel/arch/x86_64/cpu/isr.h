#pragma once

#define ISR_CTX_CTX_FLAGS_FAST_BIT  7
#define ISR_CTX_CTX_FLAGS_FAST_QWBIT    (7+8)

#define ISR_CTX_CTX_FLAGS_FIXGS_BIT 6
#define ISR_CTX_CTX_FLAGS_FIXGS_QWBIT   (6+8)

#define ISR_CTX_OFS_INTERRUPT       (18*8)
#define ISR_CTX_OFS_RBP             (17*8)

#ifndef __ASSEMBLER__

#include "types.h"
#include "initializer_list.h"

__BEGIN_DECLS

struct thread_info_t;

#define ISR_CTX_REG_GPR_n(ctx, i)       ((ctx)->gpr.r.r[(i)])
#define ISR_CTX_REG_SEG_n(ctx, i)       ((ctx)->gpr.s.r[(i)])

// isr_context->gpr.r order
#define ISR_CTX_REG_RDI(ctx)            ((ctx)->gpr.r.n.rdi)
#define ISR_CTX_REG_RSI(ctx)            ((ctx)->gpr.r.n.rsi)
#define ISR_CTX_REG_RDX(ctx)            ((ctx)->gpr.r.n.rdx)
#define ISR_CTX_REG_RCX(ctx)            ((ctx)->gpr.r.n.rcx)
#define ISR_CTX_REG_R8(ctx)             ((ctx)->gpr.r.n.r8)
#define ISR_CTX_REG_R9(ctx)             ((ctx)->gpr.r.n.r9)
#define ISR_CTX_REG_RAX(ctx)            ((ctx)->gpr.r.n.rax)
#define ISR_CTX_REG_R10(ctx)            ((ctx)->gpr.r.n.r10)
#define ISR_CTX_REG_R11(ctx)            ((ctx)->gpr.r.n.r11)
#define ISR_CTX_REG_R12(ctx)            ((ctx)->gpr.r.n.r12)
#define ISR_CTX_REG_R13(ctx)            ((ctx)->gpr.r.n.r13)
#define ISR_CTX_REG_R14(ctx)            ((ctx)->gpr.r.n.r14)
#define ISR_CTX_REG_R15(ctx)            ((ctx)->gpr.r.n.r15)
#define ISR_CTX_REG_RBX(ctx)            ((ctx)->gpr.r.n.rbx)
#define ISR_CTX_REG_RBP(ctx)            ((ctx)->gpr.r.n.rbp)

#define ISR_CTX_REG_RIP(ctx)            ((ctx)->gpr.iret.rip)
#define ISR_CTX_REG_RSP(ctx)            ((ctx)->gpr.iret.rsp)
#define ISR_CTX_REG_RFLAGS(ctx)         ((ctx)->gpr.iret.rflags)

#define ISR_CTX_REG_CS(ctx)             ((ctx)->gpr.iret.cs)
#define ISR_CTX_REG_SS(ctx)             ((ctx)->gpr.iret.ss)
#define ISR_CTX_REG_DS(ctx)             ((ctx)->gpr.s.n.ds)
#define ISR_CTX_REG_ES(ctx)             ((ctx)->gpr.s.n.es)
#define ISR_CTX_REG_FS(ctx)             ((ctx)->gpr.s.n.fs)
#define ISR_CTX_REG_GS(ctx)             ((ctx)->gpr.s.n.gs)

#define ISR_CTX_REG_CR3(ctx)            ((ctx)->gpr.cr3)

#define ISR_CTX_ERRCODE(ctx)            ((ctx)->gpr.info.error_code)
#define ISR_CTX_INTR(ctx)               ((ctx)->gpr.info.interrupt)
#define ISR_CTX_CTX_FLAGS(ctx)          ((ctx)->gpr.info.ctx_flags)

#define ISR_CTX_FPU(ctx)                ((ctx)->fpr)

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

#define ISR_CTX_SSE_XMMn_b_ptr(ctx, n)  ((ctx)->fpr->xmm[(n)].byte)
#define ISR_CTX_SSE_XMMn_w_ptr(ctx, n)  ((ctx)->fpr->xmm[(n)].word)
#define ISR_CTX_SSE_XMMn_d_ptr(ctx, n)  ((ctx)->fpr->xmm[(n)].dword)
#define ISR_CTX_SSE_XMMn_q_ptr(ctx, n)  ((ctx)->fpr->xmm[(n)].qword)

#define ISR_CTX_SSE_XMMn_b(ctx, n, i)   ((ctx)->fpr->xmm[(n)].byte[(i)])
#define ISR_CTX_SSE_XMMn_w(ctx, n, i)   ((ctx)->fpr->xmm[(n)].word[(i)])
#define ISR_CTX_SSE_XMMn_d(ctx, n, i)   ((ctx)->fpr->xmm[(n)].dword[(i)])
#define ISR_CTX_SSE_XMMn_q(ctx, n, i)   ((ctx)->fpr->xmm[(n)].qword[(i)])
#define ISR_CTX_SSE_MXCSR(ctx)          ((ctx)->fpr->mxcsr)
#define ISR_CTX_SSE_MXCSR_MASK(ctx)     ((ctx)->fpr->mxcsr_mask)

// Passed by ISR handler
struct interrupt_info_t {
    uint8_t interrupt;
    uint8_t ctx_flags;
    uint8_t reserved[6];
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

struct isr_gpr_t {
    // Parameter registers
    uintptr_t rdi;
    uintptr_t rsi;
    uintptr_t rdx;
    uintptr_t rcx;
    uintptr_t r8;
    uintptr_t r9;
    // Call clobbered registers
    uintptr_t rax;
    uintptr_t r10;
    uintptr_t r11;
    // Call preserved registers
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
    uintptr_t rbx;
    uintptr_t rbp;
};

union isr_gpr_union_t {
    isr_gpr_t n;
    uintptr_t r[15];
};

struct isr_seg_t {
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
};

union isr_seg_union_t {
    isr_seg_t n;
    uint16_t r[4];
};

// Exception handler context
struct isr_gpr_context_t {
    isr_seg_union_t s;
    uintptr_t cr3;
    isr_gpr_union_t r;
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

// Exception handler C call parameter
struct isr_context_t {
    isr_fxsave_context_t * fpr;
    isr_gpr_context_t gpr;
};

// Vector register numbers
//  0-15 ->  xmm0[127:  0]-xmm15[127:  0]  (128)
// 16-31 ->  ymm0[255:128]-ymm15[255:128]  (128)
// 32-47 ->  zmm0[511:256]-zmm15[511:256]  (256)
// 48-63 -> zmm16[511:  0]-zmm31[511:  0]  (512)
// 64-72 ->             k0-k7              ( 32)

struct isr_sse_regn_t {
    uint8_t reg_no;
    uint8_t log2_size;
};

class isr_sse_reg_parts_t {
public:
    constexpr isr_sse_reg_parts_t(std::initializer_list<isr_sse_regn_t> init)
        : count(init.size())
        , buf{}
    {
        size_t i = 0;
        for (auto const& e : init)
            buf[i++] = e;
        count = i;
    }

    static const constexpr size_t max_parts = 3;
    size_t count;
    isr_sse_regn_t buf[max_parts];
};

static inline constexpr isr_sse_reg_parts_t isr_sse_xmm_parts(size_t i)
{
    if (i < 16)
        return {
            { uint8_t(i), 4 }
        };

    return {
        { uint8_t(i - 16 + 48), 4 }
    };
}

static inline constexpr isr_sse_reg_parts_t isr_sse_ymm_parts(size_t i)
{
    if (i < 16)
        return {
            { uint8_t(i), 4 },
            { uint8_t(i + 16), 4 },
            { uint8_t(i + 32), 5 }
        };

    return {
        { uint8_t(i - 16 + 48), 6 }
    };
}

static inline constexpr isr_sse_reg_parts_t isr_sse_zmm_parts(size_t i)
{
    if (i < 16)
        return {
            { uint8_t(i), 4 },
            { uint8_t(i + 16), 4 }
        };

    return {
        { uint8_t(i - 16 + 48), 5 }
    };
}

static inline constexpr isr_sse_reg_parts_t isr_sse_k_parts(size_t i)
{
    return {
        { uint8_t(i), 4 },
        { uint8_t(i + 16), 4 }
    };

    return {
        { uint8_t(i - 16 + 48), 5 }
    };
}

// Modern xsave
void isr_save_xsaveopt(void);
void isr_save_xsavec(void);
void isr_save_xsaves(void);
void isr_save_xsave(void);
void isr_restore_xrstor(void);
void isr_restore_xrstors(void);

// Legacy fxsave
void isr_save_fxsave(void);
void isr_restore_fxrstor(void);

_noreturn
void isr_sysret64(uintptr_t rip, uintptr_t rsp, uintptr_t kernel_rsp);

isr_fxsave_context_t *isr_save_fpu_ctx64(thread_info_t *outgoing_ctx);
void isr_restore_fpu_ctx64(thread_info_t *incoming_ctx);
isr_fxsave_context_t *isr_save_fpu_ctx32(thread_info_t *outgoing_ctx);
void isr_restore_fpu_ctx32(thread_info_t *incoming_ctx);

void protection_barrier();
void protection_barrier_ibpb();

void cpu_clear_fpu();

void thread_entry(void *);

__END_DECLS
#endif
