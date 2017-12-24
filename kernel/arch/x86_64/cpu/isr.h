#pragma once
#include "types.h"

__BEGIN_DECLS


#define ISR_CTX_REG_GPR_n(ctx, i) ((ctx)->gpr.r.r[(i)])
#define ISR_CTX_REG_SEG_n(ctx, i) ((ctx)->gpr.s.r[(i)])

// isr_context->gpr.r order
#define ISR_CTX_REG_RDI(ctx)            ((ctx)->gpr.r.n.rdi)
#define ISR_CTX_REG_RSI(ctx)            ((ctx)->gpr.r.n.rsi)
#define ISR_CTX_REG_RDX(ctx)            ((ctx)->gpr.r.n.rdx)
#define ISR_CTX_REG_RCX(ctx)            ((ctx)->gpr.r.n.rcx)
#define ISR_CTX_REG_R8(ctx)             ((ctx)->gpr.r.n.r8)
#define ISR_CTX_REG_R9(ctx)             ((ctx)->gpr.r.n.r9)
#define ISR_CTX_REG_RAX(ctx)            ((ctx)->gpr.r.n.rax)
#define ISR_CTX_REG_RBX(ctx)            ((ctx)->gpr.r.n.rbx)
#define ISR_CTX_REG_R10(ctx)            ((ctx)->gpr.r.n.r10)
#define ISR_CTX_REG_R11(ctx)            ((ctx)->gpr.r.n.r11)
#define ISR_CTX_REG_R12(ctx)            ((ctx)->gpr.r.n.r12)
#define ISR_CTX_REG_R13(ctx)            ((ctx)->gpr.r.n.r13)
#define ISR_CTX_REG_R14(ctx)            ((ctx)->gpr.r.n.r14)
#define ISR_CTX_REG_R15(ctx)            ((ctx)->gpr.r.n.r15)
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

struct isr_gpr_t {
    uintptr_t rdi;
    uintptr_t rsi;
    uintptr_t rdx;
    uintptr_t rcx;
    uintptr_t r8;
    uintptr_t r9;
    uintptr_t rax;
    uintptr_t rbx;
    uintptr_t r10;
    uintptr_t r11;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
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

// Function to call after switching to another context stack
struct isr_resume_context_t {
    void (*cleanup)(void*);
    void *cleanup_arg;
};

// Exception handler C call parameter
struct isr_context_t {
    isr_resume_context_t resume;
    isr_fxsave_context_t * fpr;
    isr_gpr_context_t gpr;
};

// Modern xsave
void isr_save_xsaveopt(void);
void isr_save_xsavec(void);
void isr_save_xsave(void);
void isr_restore_xrstor(void);

// Legacy fxsave
void isr_save_fxsave(void);
void isr_restore_fxrstor(void);

// Exception handlers
extern void isr_entry_0(void);
extern void isr_entry_1(void);
extern void isr_entry_2(void);
extern void isr_entry_3(void);
extern void isr_entry_4(void);
extern void isr_entry_5(void);
extern void isr_entry_6(void);
extern void isr_entry_7(void);
extern void isr_entry_8(void);
extern void isr_entry_9(void);
extern void isr_entry_10(void);
extern void isr_entry_11(void);
extern void isr_entry_12(void);
extern void isr_entry_13(void);
extern void isr_entry_14(void);
extern void isr_entry_15(void);
extern void isr_entry_16(void);
extern void isr_entry_17(void);
extern void isr_entry_18(void);
extern void isr_entry_19(void);
extern void isr_entry_20(void);
extern void isr_entry_21(void);
extern void isr_entry_22(void);
extern void isr_entry_23(void);
extern void isr_entry_24(void);
extern void isr_entry_25(void);
extern void isr_entry_26(void);
extern void isr_entry_27(void);
extern void isr_entry_28(void);
extern void isr_entry_29(void);
extern void isr_entry_30(void);
extern void isr_entry_31(void);

// PIC IRQs
extern void isr_entry_32(void);
extern void isr_entry_33(void);
extern void isr_entry_34(void);
extern void isr_entry_35(void);
extern void isr_entry_36(void);
extern void isr_entry_37(void);
extern void isr_entry_38(void);
extern void isr_entry_39(void);
extern void isr_entry_40(void);
extern void isr_entry_41(void);
extern void isr_entry_42(void);
extern void isr_entry_43(void);
extern void isr_entry_44(void);
extern void isr_entry_45(void);
extern void isr_entry_46(void);
extern void isr_entry_47(void);

// APIC IRQs
extern void isr_entry_48(void);
extern void isr_entry_49(void);
extern void isr_entry_50(void);
extern void isr_entry_51(void);
extern void isr_entry_52(void);
extern void isr_entry_53(void);
extern void isr_entry_54(void);
extern void isr_entry_55(void);
extern void isr_entry_56(void);
extern void isr_entry_57(void);
extern void isr_entry_58(void);
extern void isr_entry_59(void);
extern void isr_entry_60(void);
extern void isr_entry_61(void);
extern void isr_entry_62(void);
extern void isr_entry_63(void);
extern void isr_entry_64(void);
extern void isr_entry_65(void);
extern void isr_entry_66(void);
extern void isr_entry_67(void);
extern void isr_entry_68(void);
extern void isr_entry_69(void);
extern void isr_entry_70(void);
extern void isr_entry_71(void);

// Software interrupts (see interrupts.h)
extern void isr_entry_72(void);
extern void isr_entry_73(void);
extern void isr_entry_74(void);
extern void isr_entry_75(void);
extern void isr_entry_76(void);
extern void isr_entry_77(void);
extern void isr_entry_78(void);
extern void isr_entry_79(void);
extern void isr_entry_80(void);
extern void isr_entry_81(void);
extern void isr_entry_82(void);
extern void isr_entry_83(void);
extern void isr_entry_84(void);
extern void isr_entry_85(void);
extern void isr_entry_86(void);
extern void isr_entry_87(void);
extern void isr_entry_88(void);
extern void isr_entry_89(void);
extern void isr_entry_90(void);
extern void isr_entry_91(void);
extern void isr_entry_92(void);
extern void isr_entry_93(void);
extern void isr_entry_94(void);
extern void isr_entry_95(void);
extern void isr_entry_96(void);
extern void isr_entry_97(void);
extern void isr_entry_98(void);
extern void isr_entry_99(void);
extern void isr_entry_100(void);
extern void isr_entry_101(void);
extern void isr_entry_102(void);
extern void isr_entry_103(void);
extern void isr_entry_104(void);
extern void isr_entry_105(void);
extern void isr_entry_106(void);
extern void isr_entry_107(void);
extern void isr_entry_108(void);
extern void isr_entry_109(void);
extern void isr_entry_110(void);
extern void isr_entry_111(void);
extern void isr_entry_112(void);
extern void isr_entry_113(void);
extern void isr_entry_114(void);
extern void isr_entry_115(void);
extern void isr_entry_116(void);
extern void isr_entry_117(void);
extern void isr_entry_118(void);
extern void isr_entry_119(void);
extern void isr_entry_120(void);
extern void isr_entry_121(void);
extern void isr_entry_122(void);
extern void isr_entry_123(void);
extern void isr_entry_124(void);
extern void isr_entry_125(void);
extern void isr_entry_126(void);
extern void isr_entry_127(void);
extern void isr_entry_128(void);
extern void isr_entry_129(void);
extern void isr_entry_130(void);
extern void isr_entry_131(void);
extern void isr_entry_132(void);
extern void isr_entry_133(void);
extern void isr_entry_134(void);
extern void isr_entry_135(void);
extern void isr_entry_136(void);
extern void isr_entry_137(void);
extern void isr_entry_138(void);
extern void isr_entry_139(void);
extern void isr_entry_140(void);
extern void isr_entry_141(void);
extern void isr_entry_142(void);
extern void isr_entry_143(void);
extern void isr_entry_144(void);
extern void isr_entry_145(void);
extern void isr_entry_146(void);
extern void isr_entry_147(void);
extern void isr_entry_148(void);
extern void isr_entry_149(void);
extern void isr_entry_150(void);
extern void isr_entry_151(void);
extern void isr_entry_152(void);
extern void isr_entry_153(void);
extern void isr_entry_154(void);
extern void isr_entry_155(void);
extern void isr_entry_156(void);
extern void isr_entry_157(void);
extern void isr_entry_158(void);
extern void isr_entry_159(void);
extern void isr_entry_160(void);
extern void isr_entry_161(void);
extern void isr_entry_162(void);
extern void isr_entry_163(void);
extern void isr_entry_164(void);
extern void isr_entry_165(void);
extern void isr_entry_166(void);
extern void isr_entry_167(void);
extern void isr_entry_168(void);
extern void isr_entry_169(void);
extern void isr_entry_170(void);
extern void isr_entry_171(void);
extern void isr_entry_172(void);
extern void isr_entry_173(void);
extern void isr_entry_174(void);
extern void isr_entry_175(void);
extern void isr_entry_176(void);
extern void isr_entry_177(void);
extern void isr_entry_178(void);
extern void isr_entry_179(void);
extern void isr_entry_180(void);
extern void isr_entry_181(void);
extern void isr_entry_182(void);
extern void isr_entry_183(void);
extern void isr_entry_184(void);
extern void isr_entry_185(void);
extern void isr_entry_186(void);
extern void isr_entry_187(void);
extern void isr_entry_188(void);
extern void isr_entry_189(void);
extern void isr_entry_190(void);
extern void isr_entry_191(void);
extern void isr_entry_192(void);
extern void isr_entry_193(void);
extern void isr_entry_194(void);
extern void isr_entry_195(void);
extern void isr_entry_196(void);
extern void isr_entry_197(void);
extern void isr_entry_198(void);
extern void isr_entry_199(void);
extern void isr_entry_200(void);
extern void isr_entry_201(void);
extern void isr_entry_202(void);
extern void isr_entry_203(void);
extern void isr_entry_204(void);
extern void isr_entry_205(void);
extern void isr_entry_206(void);
extern void isr_entry_207(void);
extern void isr_entry_208(void);
extern void isr_entry_209(void);
extern void isr_entry_210(void);
extern void isr_entry_211(void);
extern void isr_entry_212(void);
extern void isr_entry_213(void);
extern void isr_entry_214(void);
extern void isr_entry_215(void);
extern void isr_entry_216(void);
extern void isr_entry_217(void);
extern void isr_entry_218(void);
extern void isr_entry_219(void);
extern void isr_entry_220(void);
extern void isr_entry_221(void);
extern void isr_entry_222(void);
extern void isr_entry_223(void);
extern void isr_entry_224(void);
extern void isr_entry_225(void);
extern void isr_entry_226(void);
extern void isr_entry_227(void);
extern void isr_entry_228(void);
extern void isr_entry_229(void);
extern void isr_entry_230(void);
extern void isr_entry_231(void);
extern void isr_entry_232(void);
extern void isr_entry_233(void);
extern void isr_entry_234(void);
extern void isr_entry_235(void);
extern void isr_entry_236(void);
extern void isr_entry_237(void);
extern void isr_entry_238(void);
extern void isr_entry_239(void);
extern void isr_entry_240(void);
extern void isr_entry_241(void);
extern void isr_entry_242(void);
extern void isr_entry_243(void);
extern void isr_entry_244(void);
extern void isr_entry_245(void);
extern void isr_entry_246(void);
extern void isr_entry_247(void);
extern void isr_entry_248(void);
extern void isr_entry_249(void);
extern void isr_entry_250(void);
extern void isr_entry_251(void);
extern void isr_entry_252(void);
extern void isr_entry_253(void);
extern void isr_entry_254(void);
extern void isr_entry_255(void);

__noreturn
void isr_sysret64(uintptr_t rip, uintptr_t rsp);

__END_DECLS
