#include "cpu.h"
#include "mm.h"
#include "mmu.h"
#include "gdt.h"
#include "isr.h"
#include "gdt.h"
#include "control_regs.h"
#include "legacy_pic.h"
#include "legacy_pit.h"
#include "thread_impl.h"
#include "cmos.h"
#include "apic.h"
#include "cpu/cpuid.h"
#include "callout.h"
#include "printk.h"
#include "idt.h"
#include "interrupts.h"
#include "syscall.h"
#include "string.h"
#include "algorithm.h"
#include "numeric_limits.h"
#include "except.h"
#include "work_queue.h"
#include "utility.h"
#include "irq.h"
#include "nofault.h"
#include "halt.h"
#include "bootinfo.h"
#include "process.h"
#include "user_mem.h"
#include "syscall/sys_signal.h"

uint32_t default_mxcsr_mask;

HIDDEN extern uint8_t * const ___rodata_fixup_insn_st[];
HIDDEN extern uint8_t * const ___rodata_fixup_insn_en[];

// smap control

static uint8_t const stac_insn[] = {
    0x0f, 0x01, 0xcb
};

static uint8_t const clac_insn[] = {
    0x0f, 0x01, 0xca
};

// zeroall safe

static uint8_t const vzeroall_nopw_insn[] = {
    0xc5, 0xfc, 0x77, 0x66, 0x90
};

// fsgsbase

static uint8_t const rdfsbase_r13_insn[] = {
    0xf3, 0x49, 0x0f, 0xae, 0xc5
};

static uint8_t const rdgsbase_r14_insn[] = {
    0xf3, 0x49, 0x0f, 0xae, 0xce
};

static uint8_t const wrfsbase_r13_insn[] = {
    0xf3, 0x49, 0x0f, 0xae, 0xd5
};

static uint8_t const wrgsbase_r14_insn[] = {
    0xf3, 0x49, 0x0f, 0xae, 0xde
};

// xsave variations

static uint8_t const fxsave_rsi_insn[] = {
    0x0f, 0xae, 0x06    // fxsave (%rsi)
};

static uint8_t const xsaveopt_rsi_insn[] = {
    0x0f, 0xae, 0x36    // xsaveopt (%rsi)
};

static uint8_t const xsave_rsi_insn[] = {
    0x0f, 0xae, 0x26    // xsave  (%rsi)
};

static uint8_t const xsavec_rsi_insn[] = {
    0x0f, 0xc7, 0x26    // xsavec (%rsi)
};

static uint8_t const xsaves_rsi_insn[] = {
    0x0f, 0xc7, 0x2e    // xsaves (%rsi)
};

static uint8_t const fxsave64_rsi_insn[] = {
    0x48, 0x0f, 0xae, 0x06  // fxsave64 (%rsi)
};

static uint8_t const xsaveopt64_rsi_insn[] = {
    0x48, 0x0f, 0xae, 0x36  // xsaveopt64 (%rsi)
};

static uint8_t const xsave64_rsi_insn[] = {
    0x48, 0x0f, 0xae, 0x26  // xsave64 (%rsi)
};

static uint8_t const xsavec64_rsi_insn[] = {
    0x48, 0x0f, 0xc7, 0x26  // xsavec64 (%rsi)
};

static uint8_t const xsaves64_rsi_insn[] = {
    0x48, 0x0f, 0xc7, 0x2e  // xsaves64 (%rsi)
};

// xrstor variations

static uint8_t const fxrstor_rsi_insn[] = {
    0x0f, 0xae, 0x0e    // fxrstor (%rsi)
};

static uint8_t const xrstor_rsi_insn[] = {
    0x0f, 0xae, 0x2e    // xrstor (%rsi)
};

static uint8_t const xrstors_rsi_insn[] = {
    0x0f, 0xc7, 0x1e    // xrstors (%rsi)
};

static uint8_t const xrstor64_rsi_insn[] = {
    0x48, 0x0f, 0xae, 0x2e  // xrstor64 (%rsi)
};

static uint8_t const xrstors64_rsi_insn[] = {
    0x48, 0x0f, 0xc7, 0x1e  // xrstors64 (%rsi)
};

static uint8_t const fxrstor64_rsi_insn[] = {
    0x48, 0x0f, 0xae, 0x0e  // fxrstor64 (%rsi)
};

static uint8_t const jmp_disp32_insn[] = {
    0xe9, 0x00, 0x00, 0x00, 0x00  // jmp further_than_signed_byte_displacement
};

static uint8_t const data16_prefix[] = {
    0x66
};

static uint8_t const ds_prefix[] = {
    0x3E
};

static uint8_t const call_disp32_insn[] = {
    0xE8
};

static uint8_t const *xsave64_insn(xsave_support_t support)
{
    switch (support) {
    default:
    case xsave_support_t::FXSAVE: return fxsave64_rsi_insn;
    case xsave_support_t::XSAVEOPT: return xsaveopt64_rsi_insn;
    case xsave_support_t::XSAVE: return xsave64_rsi_insn;
    case xsave_support_t::XSAVEC: return xsavec64_rsi_insn;
    case xsave_support_t::XSAVES: return xsaves64_rsi_insn;
    }
}

static size_t xsave64_sz(xsave_support_t support)
{
    switch (support) {
    default:
    case xsave_support_t::FXSAVE: return sizeof(fxsave64_rsi_insn);
    case xsave_support_t::XSAVEOPT: return sizeof(xsaveopt64_rsi_insn);
    case xsave_support_t::XSAVE: return sizeof(xsave64_rsi_insn);
    case xsave_support_t::XSAVEC: return sizeof(xsavec64_rsi_insn);
    case xsave_support_t::XSAVES: return sizeof(xsaves64_rsi_insn);
    }
}

static uint8_t const *xsave32_insn(xsave_support_t support)
{
    switch (support) {
    default:
    case xsave_support_t::FXSAVE: return fxsave_rsi_insn;
    case xsave_support_t::XSAVEOPT: return xsaveopt_rsi_insn;
    case xsave_support_t::XSAVE: return xsave_rsi_insn;
    case xsave_support_t::XSAVEC: return xsavec_rsi_insn;
    case xsave_support_t::XSAVES: return xsaves_rsi_insn;
    }
}

static size_t xsave32_sz(xsave_support_t support)
{
    switch (support) {
    default:
    case xsave_support_t::FXSAVE: return sizeof(fxsave_rsi_insn);
    case xsave_support_t::XSAVEOPT: return sizeof(xsaveopt_rsi_insn);
    case xsave_support_t::XSAVE: return sizeof(xsave_rsi_insn);
    case xsave_support_t::XSAVEC: return sizeof(xsavec_rsi_insn);
    case xsave_support_t::XSAVES: return sizeof(xsaves_rsi_insn);
    }
}

static uint8_t const *xrstor64_insn(xsave_support_t support)
{
    switch (support) {
    default:
    case xsave_support_t::FXSAVE: return fxrstor64_rsi_insn;
    case xsave_support_t::XSAVEOPT: // fall through
    case xsave_support_t::XSAVE: // fall through
    case xsave_support_t::XSAVEC: return xrstor64_rsi_insn;
    case xsave_support_t::XSAVES: return xrstors64_rsi_insn;
    }
}

static size_t xrstor64_sz(xsave_support_t support)
{
    switch (support) {
    default:
    case xsave_support_t::FXSAVE: return sizeof(fxrstor64_rsi_insn);
    case xsave_support_t::XSAVEOPT: // fall through
    case xsave_support_t::XSAVE: // fall through
    case xsave_support_t::XSAVEC: return sizeof(xrstor64_rsi_insn);
    case xsave_support_t::XSAVES: return sizeof(xrstors64_rsi_insn);
    }
}

static uint8_t const *xrstor32_insn(xsave_support_t support)
{
    switch (support) {
    default:
    case xsave_support_t::FXSAVE: return fxrstor_rsi_insn;
    case xsave_support_t::XSAVE: // fall through
    case xsave_support_t::XSAVEOPT: // fall through
    case xsave_support_t::XSAVEC: return xrstor_rsi_insn;
    case xsave_support_t::XSAVES: return xrstors_rsi_insn;
    }
}

static size_t xrstor32_sz(xsave_support_t support)
{
    switch (support) {
    default:
    case xsave_support_t::FXSAVE: return sizeof(fxrstor_rsi_insn);
    case xsave_support_t::XSAVEOPT: // fall through
    case xsave_support_t::XSAVE: // fall through
    case xsave_support_t::XSAVEC: return sizeof(xrstor_rsi_insn);
    case xsave_support_t::XSAVES: return sizeof(xrstors_rsi_insn);
    }
}

// These preserve all registers like the instruction they replace
extern "C" void soft_vzeroall();
extern "C" void soft_rdfsgsbase_r13r14();
extern "C" void soft_wrfsgsbase_r13r14();
extern "C" void soft_rdfsbase_r13();
extern "C" void soft_rdgsbase_r14();
extern "C" void soft_wrfsbase_r13();
extern "C" void soft_wrgsbase_r14();

bool constexpr force_msr_fsgsbase = true;

extern "C"
KERNEL_API void cpu_apply_fixups(uint8_t * const *rodata_st,
                                 uint8_t * const *rodata_en)
{
    // Perform code patch fixups
    for (uint8_t * const *fixup_ptr = rodata_st;
         fixup_ptr < rodata_en; ++fixup_ptr) {
        uint8_t *code = *fixup_ptr;

        intptr_t addr = uintptr_t(code);
        intptr_t next = 0;

        intptr_t dist = 0;
        size_t old_sz = 0;
        size_t new_sz = 0;

        switch (*code) {
        case 0x67:
            // fall through
        case 0xE8:
            // Call instruction
            int32_t disp32;
            int32_t operand_ofs;
            old_sz = 5;
            operand_ofs = 1;

            // Handle relaxed indirect call
            bool is_relaxed_indirect;
            is_relaxed_indirect = (*code == 0x67);
            operand_ofs += is_relaxed_indirect;

            next = addr + old_sz;
            disp32 = 0;
            memcpy(&disp32, code + operand_ofs, sizeof(disp32));

            intptr_t dest_addr;
            dest_addr = next + disp32;

            if (dest_addr == intptr_t(protection_barrier_from_user)) {
                // Prevent user code influencing anything
                if (cpuid_has_ibpb()) {
                    // Call ibpb function
                    printdbg("Using IBPB mitigation\n");
                    code += is_relaxed_indirect;
                    new_sz = 5;
                    dist = intptr_t(protection_barrier_from_user_ibpb) - next;
                } else {
                    // nop it out
                    printdbg("Not using IBPB mitigation\n");
                    new_sz = 0;
                }

                break;
            } else if (dest_addr == intptr_t(protection_barrier_to_user)) {
                // Prevent any information leaking back to user mode
                // Clear microarchitectural data (intel)
                if (cpuid_has_md_clear()) {
                    // Call verw function
                    printdbg("Using MDS mitigation\n");
                    code += is_relaxed_indirect;
                    new_sz = 5;
                    dist = intptr_t(protection_barrier_to_user_verw) - next;
                } else {
                    // nop it out
                    printdbg("Not using MDS mitigation\n");
                    new_sz = 0;
                }

                break;
            }

            printdbg("Ignoring useless or unrecognized"
                     " insn_fixup on call at %#zx\n",
                     intptr_t(code));

            continue;

        case 0xF3:
            // rd/wr fs/gs base

            //
            // Read and write fsbase r13

            if (unlikely(!memcmp(code, rdfsbase_r13_insn,
                                sizeof(rdfsbase_r13_insn)) &&
                         !memcmp(code + sizeof(rdfsbase_r13_insn),
                                 rdgsbase_r14_insn,
                                 sizeof(rdgsbase_r14_insn)))) {
                // Both at once in one
                if (!force_msr_fsgsbase && cpuid_has_fsgsbase())
                    continue;

                old_sz = sizeof(rdfsbase_r13_insn) +
                        sizeof(rdgsbase_r14_insn);
                new_sz = 5;
                next = addr + new_sz;
                dist = uintptr_t(soft_rdfsgsbase_r13r14) - next;
            } else if (unlikely(!memcmp(code, wrfsbase_r13_insn,
                                        sizeof(wrfsbase_r13_insn)) &&
                                !memcmp(code + sizeof(wrfsbase_r13_insn),
                                        wrgsbase_r14_insn,
                                        sizeof(wrgsbase_r14_insn)))) {
                // Both at once in one
                if (!force_msr_fsgsbase && cpuid_has_fsgsbase())
                    continue;

                old_sz = sizeof(wrfsbase_r13_insn) +
                        sizeof(wrgsbase_r14_insn);
                new_sz = 5;
                next = addr + new_sz;
                dist = uintptr_t(soft_wrfsgsbase_r13r14) - next;
            } else if (unlikely(!memcmp(code, rdfsbase_r13_insn,
                                        sizeof(rdfsbase_r13_insn)))) {
                if (!force_msr_fsgsbase && cpuid_has_fsgsbase())
                    continue;

                old_sz = sizeof(rdfsbase_r13_insn);
                new_sz = 5;
                next = addr + new_sz;
                dist = uintptr_t(soft_rdfsbase_r13) - next;
            } else if (unlikely(!memcmp(code, wrfsbase_r13_insn,
                                        sizeof(wrfsbase_r13_insn)))) {
                if (!force_msr_fsgsbase && cpuid_has_fsgsbase())
                    continue;

                old_sz = sizeof(wrfsbase_r13_insn);
                new_sz = 5;
                next = addr + new_sz;
                dist = uintptr_t(soft_wrfsbase_r13) - next;
            } else if (unlikely(!memcmp(code, rdgsbase_r14_insn,
                                        sizeof(rdgsbase_r14_insn)))) {
                if (!force_msr_fsgsbase && cpuid_has_fsgsbase())
                    continue;

                old_sz = sizeof(rdgsbase_r14_insn);
                new_sz = 5;
                next = addr + new_sz;
                dist = uintptr_t(soft_rdgsbase_r14) - next;
            } else if (unlikely(!memcmp(code, wrgsbase_r14_insn,
                                        sizeof(wrgsbase_r14_insn)))) {
                if (!force_msr_fsgsbase && cpuid_has_fsgsbase())
                    continue;

                old_sz = sizeof(wrgsbase_r14_insn);
                new_sz = 5;
                next = addr + new_sz;
                dist = uintptr_t(soft_wrgsbase_r14) - next;
            }
            break;

        case 0xC5:
            if (unlikely(!memcmp(code, vzeroall_nopw_insn,
                                 sizeof(vzeroall_nopw_insn)))) {
                if (cpuid_has_avx())
                    continue;

                old_sz = sizeof(vzeroall_nopw_insn);
                new_sz = 5;
                next = addr + new_sz;
                dist = uintptr_t(soft_vzeroall) - next;
            }
            break;

        case 0x0f:
            // 32 bit fxsave/fxrstor and stac/clac
            if (unlikely(!memcmp(code, fxsave_rsi_insn,
                                 sizeof(fxsave_rsi_insn)))) {
                old_sz = xsave32_sz(xsave_support_t::FXSAVE);
                new_sz = xsave32_sz(xsave_support);
                cpu_patch_code(code, xsave32_insn(xsave_support),
                               xsave32_sz(xsave_support));
                break;
            } else if (unlikely(!memcmp(code, fxrstor_rsi_insn,
                                        sizeof(fxrstor_rsi_insn)))) {
                old_sz = xrstor32_sz(xsave_support_t::FXSAVE);
                new_sz = xrstor32_sz(xsave_support);
                cpu_patch_code(code, xrstor32_insn(xsave_support),
                               xrstor32_sz(xsave_support));
                break;
            } else if (unlikely(!memcmp(code, stac_insn,
                                        sizeof(stac_insn)))) {
                if (cpuid_has_smap())
                    continue;

                old_sz = sizeof(stac_insn);
                new_sz = 0;
                break;
            } else if (unlikely(!memcmp(code, clac_insn,
                                        sizeof(clac_insn)))) {
                if (cpuid_has_smap())
                    continue;

                old_sz = sizeof(clac_insn);
                new_sz = 0;
                break;
            }
            break;

        case 0x48:
            // 64-bit fxsave64/fxrstor64
            if (unlikely(!memcmp(code, fxsave64_rsi_insn,
                                 sizeof(fxsave64_rsi_insn)))) {
                old_sz = xsave64_sz(xsave_support_t::FXSAVE);
                new_sz = xsave64_sz(xsave_support);
                cpu_patch_code(code, xsave64_insn(xsave_support),
                               xsave64_sz(xsave_support));
                break;
            } else if (unlikely(!memcmp(code, fxrstor64_rsi_insn,
                                        sizeof(fxrstor64_rsi_insn)))) {
                old_sz = xrstor64_sz(xsave_support_t::FXSAVE);
                new_sz = xrstor64_sz(xsave_support);
                cpu_patch_code(code, xrstor64_insn(xsave_support),
                               xrstor64_sz(xsave_support));
                break;
            }
            break;

        }

        if (!dist && new_sz == old_sz)
            continue;

        assert(new_sz <= old_sz);

        // Disable write protection and interrupts temporarily
        cpu_scoped_irq_disable irq_dis;
        cpu_scoped_wp_disable wp_dis;

        // Replace with a call
        if (dist) {
            // Relocation truncated to fit? Doubt it but unhandled if so
            if (unlikely(dist < INT32_MIN || dist > INT32_MAX))
                panic("Relocation would be truncated to fit");

            code[0] = call_disp32_insn[0];

            // Convert it first so the memcpy isn't assuming endianness
            int32_t dist32 = int32_t(dist);

            memcpy(code + 1, &dist32, sizeof(dist32));
        }

        if (old_sz > new_sz)
            cpu_patch_nop(code + new_sz, old_sz - new_sz);
    }
}

void cpu_init_early(int ap)
{
    if (ap)
        mm_set_master_pagedir();

    gdt_init(ap);

    // These two must agree, always (except ring 0 allows null ss)
    cpu_ss_set(GDT_SEL_KERNEL_DATA);
    cpu_cs_set(GDT_SEL_KERNEL_CODE64);

    // The CPU doesn't care about any of these registers in long mode
    // So, might as well leave them
    // permanently set to compatibility mode values
    cpu_ds_set(GDT_SEL_USER_DATA | 3);
    cpu_es_set(GDT_SEL_USER_DATA | 3);
    cpu_fs_set(GDT_SEL_USER_DATA | 3);
    cpu_swapgs();
    cpu_gs_set(GDT_SEL_USER_DATA | 3);
    cpu_swapgs();

    thread_cls_init_early(ap);

    gdt_init_tss_early();
    idt_init(ap);

    // Enable SSE early
    cpu_cr4_change_bits(0, CPU_CR4_OFXSR | CPU_CR4_OSXMMEX);

    cpu_fninit();

    if (!ap) {
        // Detect the MXCSR mask from fxsave context
        isr_fxsave_context_t __aligned(64) fxctx;
        cpu_fxsave64(&fxctx);
        default_mxcsr_mask = fxctx.mxcsr_mask
                ? fxctx.mxcsr_mask
                : 0xFFBF;
    }

    cpu_mxcsr_set(CPU_MXCSR_ELF_INIT & default_mxcsr_mask);
    cpu_fcw_set(CPU_FPUCW_ELF_INIT);

    // Configure xsave
    if (cpuid_has_xsave()) {
        if (!ap) {
            cpuid_t info;
            if (cpuid(&info, CPUID_INFO_XSAVE, 0)) {
                xsave_supported_states = info.eax;
                xsave_enabled_states = info.eax &
                        (XCR0_X87 | XCR0_SSE | XCR0_AVX | XCR0_AVX512_UPPER |
                         XCR0_AVX512_XREGS | XCR0_AVX512_OPMASK);
            }
        }

        cpu_cr4_change_bits(0, CPU_CR4_OSXSAVE);
        cpu_xcr_change_bits(0, ~xsave_supported_states, xsave_enabled_states);
    }

    idt_xsave_detect(ap);

    if (!ap)
        cpu_apply_fixups(___rodata_fixup_insn_st, ___rodata_fixup_insn_en);
}

_constructor(ctor_cpu_init_bsp) static void cpu_init_bsp()
{
    cpu_init(0);
}

static bool cpu_signal_copy_fpu_ctx(uintptr_t user_addr, bool use64)
{
    char data[4096];

    assert(sse_context_size < sizeof(data));

    uintptr_t addr = uintptr_t(data + sizeof(data));

    if (cpuid_has_xsave()) {
        size_t ctx_sz = sse_context_size;

        addr -= ctx_sz;
        addr &= -64;

        isr_xsave_context_t *ctx = (isr_xsave_context_t*)addr;

        // The initial content of the memory matters
        memset(ctx, 0, sizeof(*ctx));

        if (use64)
            cpu_xsave64(ctx);
        else
            cpu_xsave32(ctx);

        if (unlikely(!mm_copy_user((void*)user_addr, ctx, ctx_sz)))
            return false;
    } else {
        addr -= sizeof(isr_fxsave_context_t);
        addr &= -16;

        isr_fxsave_context_t *ctx = (isr_fxsave_context_t*)addr;

        if (use64)
            cpu_fxsave64(ctx);
        else
            cpu_fxsave32(ctx);

        if (unlikely(!mm_copy_user((void*)user_addr, ctx,
                                   sizeof(isr_fxsave_context_t))))
            return false;
    }

    return true;
}

extern "C" isr_context_t *cpu_signal_dispatcher(int intr, isr_context_t *ctx)
{
    // If it is kernel code, unhandled
    if (unlikely(!GDT_SEL_RPL_IS_USER(ISR_CTX_REG_CS(ctx))))
        return nullptr;

    // Lookup restorer
    process_t *process = fast_cur_process();

    if (unlikely(!process))
        return nullptr;

    __sig_restorer_t restorer = process_get_restorer(process);

    if (unlikely(!restorer))
        return nullptr;

    int sig = -1;
    int code = -1;

    unsigned fsw;
    unsigned fcw;
    unsigned fex;

    //FPE_FLTRES   fp_inexact − floating-point inexact result
    //FPE_FLTDIV   fp_division − floating-point division by zero
    //FPE_FLTUND   fp_underflow − floating-point underflow
    //FPE_FLTOVF   fp_overflow − floating-point overflow
    //FPE_FLTINV   fp_invalid − floating-point invalid operation
    switch (intr) {
    case INTR_EX_DIV:
        code = FPE_INTDIV;
        sig = SIGFPE;
        break;

    case INTR_EX_BOUND:
        // Subscript out of range
        code = FPE_FLTSUB;
        sig = SIGSEGV;
        break;

    case INTR_EX_SIMD:
    case INTR_EX_MATH:
        fsw = cpu_fsw_get();
        fcw = cpu_fcw_get();

        // Calculate which exceptions might have caused this interrupt
        fex = CPU_FPUCW_IM | CPU_FPUCW_DM | CPU_FPUCW_ZM |
                CPU_FPUCW_OM | CPU_FPUCW_IM | CPU_FPUCW_PM;
        fex = fsw & (fcw & fex);

        // Precision error
        if (fex & CPU_FPUSW_PE_BIT)
            code = FPE_FLTRES;
        else if (fex & CPU_FPUSW_OE_BIT)
            code = FPE_FLTOVF;
        else if (fex & CPU_FPUSW_UE_BIT)
            code = FPE_FLTUND;
        else if (fex & CPU_FPUSW_IE_BIT)
            code = FPE_FLTINV;

        sig = SIGFPE;
        break;

    case INTR_EX_OVF:
        code = FPE_INTOVF;
        sig = SIGFPE;
        break;

    case INTR_EX_DEBUG:
    case INTR_EX_BREAKPOINT:
        sig = SIGTRAP;
        break;

    case INTR_EX_OPCODE:
        sig = SIGILL;
        break;

    case INTR_EX_SEGMENT:
    case INTR_EX_TSS:
    case INTR_EX_STACK:
    case INTR_EX_GPF:
    case INTR_EX_PAGE:
        sig = SIGSEGV;
        break;

    case INTR_EX_DEV_NOT_AV:
        break;

    case INTR_EX_DBLFAULT:
    case INTR_EX_COPR_SEG:
    case INTR_EX_ALIGNMENT:
        sig = SIGBUS;
        break;
    }

start_over:
    siginfo_t info{};

    info.si_signo = sig;
    info.si_status = sig;
    info.si_code = code;
    info.si_addr = (void*)ISR_CTX_REG_RIP(ctx);
    info.si_pid = process->pid;
    info.si_uid = process->uid;

    bool use64 = GDT_SEL_IS_C64(ISR_CTX_REG_CS(ctx));

    uintptr_t rsp = ISR_CTX_REG_RSP(ctx);

    // Skip over possible red zone and align
    rsp -= 128;
    rsp &= -16;

    uintptr_t stack_data_en = rsp;

    // Make room for fxsave context
    rsp -= ext::max(sizeof(mcontext_x86_fpu_t), sse_context_size);

    // 512-bit alignment for xsave, 128-bit alignment for fxsave
    if (likely(cpuid_has_xsave() && sse_context_size < 4096))
        rsp &= -64;
    else
        rsp &= -16;

    uintptr_t fpu_ptr = rsp;

    // Make room for siginfo, and align
    rsp -= sizeof(info);
    rsp &= -16;

    uintptr_t info_ptr = rsp;

    // Make room for appropriate sized context and align
    rsp -= use64 ? sizeof(mcontext_t) : sizeof(mcontext32_t);
    rsp &= -16;

    uintptr_t mctx_ptr = rsp;

    uintptr_t stack_data_st = rsp;
    uintptr_t stack_data_sz = stack_data_en - stack_data_st;

    // Make sure the siginfo buffer is okay
    // (from end of last thing to start of first thing)
    if (unlikely(!mm_is_user_range((void*)stack_data_st, stack_data_sz))) {
        if (sig != SIGSEGV) {
            sig = SIGSEGV;
            goto start_over;
        }

        return nullptr;
    }

    if (unlikely(!cpu_signal_copy_fpu_ctx(fpu_ptr, use64))) {
        if (sig != SIGSEGV) {
            sig = SIGSEGV;
            goto start_over;
        }

        return nullptr;
    }

    // Copy siginfo into userspace stack
    if (unlikely(!mm_copy_user((void*)info_ptr, &info, sizeof(info)))) {
        if (sig != SIGSEGV) {
            sig = SIGSEGV;
            goto start_over;
        }

        return nullptr;
    }

    if (GDT_SEL_IS_C64(ISR_CTX_REG_CS(ctx))) {
        mcontext_t ctx64{};

        ctx64.__fpu = fpu_ptr;

        ctx64.__regs[R_RDI] = ISR_CTX_REG_RDI(ctx);
        ctx64.__regs[R_RSI] = ISR_CTX_REG_RSI(ctx);
        ctx64.__regs[R_RDX] = ISR_CTX_REG_RDX(ctx);
        ctx64.__regs[R_RCX] = ISR_CTX_REG_RCX(ctx);
        ctx64.__regs[R_R8]  =  ISR_CTX_REG_R8(ctx);
        ctx64.__regs[R_R9]  =  ISR_CTX_REG_R9(ctx);
        ctx64.__regs[R_RAX] = ISR_CTX_REG_RAX(ctx);
        ctx64.__regs[R_R10] = ISR_CTX_REG_R10(ctx);
        ctx64.__regs[R_R11] = ISR_CTX_REG_R11(ctx);
        ctx64.__regs[R_R12] = ISR_CTX_REG_R12(ctx);
        ctx64.__regs[R_R13] = ISR_CTX_REG_R13(ctx);
        ctx64.__regs[R_R14] = ISR_CTX_REG_R14(ctx);
        ctx64.__regs[R_R15] = ISR_CTX_REG_R15(ctx);
        ctx64.__regs[R_RBX] = ISR_CTX_REG_RBX(ctx);
        ctx64.__regs[R_RBP] = ISR_CTX_REG_RBP(ctx);

        ctx64.__rip = (uint64_t)(uintptr_t)ISR_CTX_REG_RIP(ctx);
        ctx64.__cs = ISR_CTX_REG_CS(ctx);
        ctx64.__rflags = ISR_CTX_REG_RFLAGS(ctx);
        ctx64.__rsp = ISR_CTX_REG_RSP(ctx);
        ctx64.__ss = ISR_CTX_REG_SS(ctx);

        if (unlikely(!mm_copy_user((void*)mctx_ptr, &ctx64, sizeof(ctx64))))
            return nullptr;
    } else {
        mcontext32_t ctx32{};

        ctx32.__fpu = fpu_ptr;

        ctx32.__regs[R_EAX] = uint32_t(ISR_CTX_REG_RAX(ctx));
        ctx32.__regs[R_EDX] = uint32_t(ISR_CTX_REG_RDX(ctx));
        ctx32.__regs[R_ECX] = uint32_t(ISR_CTX_REG_RCX(ctx));
        ctx32.__regs[R_ESI] = uint32_t(ISR_CTX_REG_RSI(ctx));
        ctx32.__regs[R_EDI] = uint32_t(ISR_CTX_REG_RDI(ctx));
        ctx32.__regs[R_EBX] = uint32_t(ISR_CTX_REG_RBX(ctx));
        ctx32.__regs[R_EBP] = uint32_t(ISR_CTX_REG_RBP(ctx));

        ctx32.__eip = (uint32_t)(uintptr_t)ISR_CTX_REG_RIP(ctx);
        ctx32.__cs = (uint32_t)ISR_CTX_REG_CS(ctx);
        ctx32.__eflags = (uint32_t)ISR_CTX_CTX_FLAGS(ctx);
        ctx32.__esp = (uint32_t)ISR_CTX_REG_RSP(ctx);
        ctx32.__ss = (uint32_t)ISR_CTX_REG_SS(ctx);

        if (unlikely(!mm_copy_user((void*)mctx_ptr, &ctx32, sizeof(ctx32))))
            return nullptr;
    }

    // Point rsp at mcontext(32)_t
    // and instruction pointer at start of restorer (signal trampoline)
    ISR_CTX_REG_RSP(ctx) = mctx_ptr;
    ISR_CTX_REG_RIP(ctx) = (intptr_t(*)(void*))(void*)restorer;

    // Three parameters: int, siginfo_t *, void (*)(int, siginfo_t *, void*)
    if (likely(use64)) {
        ISR_CTX_REG_RDI(ctx) = sig;
        ISR_CTX_REG_RSI(ctx) = info_ptr;
        ISR_CTX_REG_RDX(ctx) = (uintptr_t)process->sighand[sig].sa_sigaction;
    } else {
        ISR_CTX_REG_RAX(ctx) = (uint32_t)sig;
        ISR_CTX_REG_RCX(ctx) = (uint32_t)info_ptr;
        ISR_CTX_REG_RDX(ctx) = (uint32_t)(uintptr_t)
                process->sighand[sig].sa_sigaction;
    }

    // Continue execution with modified context
    return ctx;
}

extern "C" isr_context_t *cpu_gpf_handler(int intr, isr_context_t *ctx)
{
    // Handle nofault functions
    if (nofault_ip_in_range(uintptr_t(ISR_CTX_REG_RIP(ctx)))) {
        //
        // Fault occurred in nofault code region

        // Figure out which landing pad to use
        uintptr_t landing_pad = nofault_find_landing_pad(
                    uintptr_t(ISR_CTX_REG_RIP(ctx)));

        if (likely(landing_pad)) {
            // Put CPU on landing pad with rax == -1
            ISR_CTX_REG_RAX(ctx) = -1;
            ISR_CTX_REG_RIP(ctx) = (intptr_t(*)(void*))landing_pad;
            return ctx;
        }
    } else if (GDT_SEL_RPL_IS_USER(ISR_CTX_REG_CS(ctx))) {
        // Inject a signal

        return cpu_signal_dispatcher(intr, ctx);
    }

    return nullptr;
}

_noreturn
static isr_context_t *cpu_panic_handler(int intr, isr_context_t *ctx)
{
    halt_forever();
}

_noreturn
void cpu_init_ap()
{
    uint32_t cpu_nr = thread_cpu_number();

    cpu_init_early(cpu_nr);
    cpu_init(1);
    apic_init(1);
    thread_init(1);
    __builtin_unreachable();
}

void cpu_init(int ap)
{
    cpu_cr0_change_bits(
                // EM = 0 (No FPU emulation)
                // CD = 0 (Do not disable cache)
                // NW = 0 (Writeback cache enabled)
                CPU_CR0_EM | CPU_CR0_CD | CPU_CR0_NW,
                // TS = 1 (Lock out FPU, raise #NM if used)
                // MP = 1 (Monitor coprocessor task switched)
                // WP = 1 (Enable write protection in CPL 0)
                // ET = 1 (FPU is not an 80287)
                // NE = 1 (Native FPU error handling, no IRQ)
                // AM = 1 (Allow EFLAGS AC to enable alignment checks in CPL 3)
                CPU_CR0_TS | CPU_CR0_MP | CPU_CR0_WP |
                CPU_CR0_ET | CPU_CR0_NE | CPU_CR0_AM);

    uintptr_t set = 0;
    uintptr_t clr = 0;

    // Supervisor Mode Execution Prevention (SMEP)
    if (cpuid_has_smep())
        set |= CPU_CR4_SMEP;

    // Supervisor Mode Access Prevention (SMAP)
    if (cpuid_has_smap())
        set |= CPU_CR4_SMAP;

    // Allow access to rdtsc in user mode
    set |= CPU_CR4_PCE;

    // Enable global pages feature if available
    if (cpuid_has_pge())
        set |= CPU_CR4_PGE;

    // Disable 36 bit paging
    clr |= CPU_CR4_PSE;

    // Enable debugging extensions feature if available
    if (cpuid_has_de())
        set |= CPU_CR4_DE;

    // Disable paging context identifiers feature
    clr |= CPU_CR4_PCIDE;

//    // Enable {RD|WR}{FS|GS}BASE instructions
//    if (cpuid_has_fsgsbase())
//        set |= CPU_CR4_FSGSBASE;

    // Disable {RD|WR}{FS|GS}BASE instructions
    clr |= CPU_CR4_FSGSBASE;

    if (cpuid_has_umip()) {
        // Enable user mode instruction prevention
        // Prevents execution of SGDT, SIDT, SLDT, SMSW, STR in user mode
        set |= CPU_CR4_UMIP;
    }

    // Enable SSE and SSE exceptions
    set |= CPU_CR4_OFXSR | CPU_CR4_OSXMMEX;

    // Do not bother disabling rdstc, it is futile in the presence of threads
    clr |= CPU_CR4_TSD;

    cpu_cr4_change_bits(clr, set);

    //
    // Adjust IA32_MISC_ENABLES

    // Enable enhanced rep move string
    if (cpuid_has_erms())
        cpu_msr_change_bits(CPU_MSR_MISC_ENABLE, 0,
                            CPU_MSR_MISC_ENABLE_FAST_STR);

    //    if (cpuid_is_intel()) {
    //        clr = CPU_MSR_MISC_ENABLE_LIMIT_CPUID;

    //        if (cpuid_has_mwait())
    //            set = CPU_MSR_MISC_ENABLE_MONITOR_FSM;

    //        if (cpuid_has_nx())
    //            clr = CPU_MSR_MISC_ENABLE_XD_DISABLE;

    //        cpu_msr_change_bits(CPU_MSR_MISC_ENABLE, clr, set);
    //    }

    // Enable syscall/sysret
    set = CPU_MSR_EFER_SCE;
    clr = 0;

    // Enable no-execute if feature available
    if (cpuid_has_nx())
        set |= CPU_MSR_EFER_NX;

    cpu_msr_change_bits(CPU_MSR_EFER, clr, set);

    // Configure syscall
    // Clear the alignment check, direction flag, trap flag, interrupt flag,
    // resume flag, v86 flag
    cpu_msr_set(CPU_MSR_FMASK, CPU_EFLAGS_AC | CPU_EFLAGS_DF |
                CPU_EFLAGS_TF | CPU_EFLAGS_IF | CPU_EFLAGS_RF |
                CPU_EFLAGS_VM);
    // Set 64 bit syscall kernel entry point
    cpu_msr_set(CPU_MSR_LSTAR, uintptr_t(syscall_entry));
    cpu_msr_set(CPU_MSR_CSTAR, uintptr_t(syscall32_entry));
    // syscall CS/SS at bit 48, sysret CS/SS at bit 32
    cpu_msr_set(CPU_MSR_STAR,
                (uint64_t(GDT_SEL_KERNEL_CODE64) << 32) |
                (uint64_t(GDT_SEL_USER_CODE32 | 3) << 48));


    // SYSCALL and SYSRET are hardwired to assume these things about the GDT:
    static_assert(GDT_SEL_USER_CODE64 == GDT_SEL_USER_CODE32 + 16,
                  "GDT inconsistent with SYSCALL/SYSRET behaviour");
    static_assert(GDT_SEL_USER_DATA == GDT_SEL_USER_CODE32 + 8,
                  "GDT inconsistent with SYSCALL/SYSRET behaviour");
    static_assert(GDT_SEL_USER_CODE64 == GDT_SEL_USER_DATA + 8,
                  "GDT inconsistent with SYSCALL/SYSRET behaviour");
    static_assert(GDT_SEL_KERNEL_DATA == GDT_SEL_KERNEL_CODE64 + 8,
                  "GDT inconsistent with SYSCALL/SYSRET behaviour");

    // Configure sysenter
    if (!cpuid_is_amd()) {
        cpu_msr_set(CPU_MSR_SYSENTER_CS, 0);
        cpu_msr_set(CPU_MSR_SYSENTER_EIP, uint64_t(0));
        cpu_msr_set(CPU_MSR_SYSENTER_ESP, 0);
    }

    // Configure security features/workarounds
    clr = 0;
    set = 0;

    // Try to resort to IBRS if IBPB not supported
    // If we can't flush branch prediction caches then make CPU segregate
    if (!cpuid_has_ibpb() && cpuid_has_ibrs())
        set |= CPU_MSR_SPEC_CTRL_IBRS;

    // Enable peer threads to have separate indirect branch predictors
    if (cpuid_has_stibp())
        set |= CPU_MSR_SPEC_CTRL_STIBP;

    // Enable blocking speculative stores until prior loads are resolved
    if (cpuid_has_ssbd())
        set |= CPU_MSR_SPEC_CTRL_SSBD;

    if (set)
        cpu_msr_change_bits(CPU_MSR_SPEC_CTRL, clr, set);

    // Load null LDT
    cpu_ldt_set(0);

    intr_hook(INTR_EX_DIV, cpu_signal_dispatcher, "cpu_div_err", eoi_none);
    intr_hook(INTR_EX_STACK, cpu_signal_dispatcher, "cpu_stack_err", eoi_none);
    intr_hook(INTR_EX_MATH, cpu_signal_dispatcher, "cpu_x87_err", eoi_none);
    intr_hook(INTR_EX_OVF, cpu_signal_dispatcher, "cpu_overflow_err", eoi_none);
    intr_hook(INTR_EX_DEV_NOT_AV, cpu_signal_dispatcher, "cpu_fpu_na_err", eoi_none);
    intr_hook(INTR_EX_OPCODE, cpu_signal_dispatcher, "cpu_opcode_err", eoi_none);
    intr_hook(INTR_EX_BOUND, cpu_signal_dispatcher, "cpu_bound_err", eoi_none);
    intr_hook(INTR_EX_SIMD, cpu_signal_dispatcher, "cpu_simd_err", eoi_none);
    intr_hook(INTR_EX_SEGMENT, cpu_signal_dispatcher, "cpu_segment_err", eoi_none);
    intr_hook(INTR_EX_ALIGNMENT, cpu_signal_dispatcher, "cpu_align_err", eoi_none);

    intr_hook(INTR_EX_GPF, cpu_gpf_handler, "cpu_gpf_handler", eoi_none);

    intr_hook(INTR_IPI_PANIC, cpu_panic_handler,
              "cpu_panic_handler", eoi_lapic);
}

_constructor(ctor_cpu_hw_init) static void cpu_hw_init_bsp()
{
    cpu_hw_init(0);
}

void cpu_hw_init(int ap)
{
    printk("Initializing PIT\n");

    // May need PIT nsleep early for APIC calibration
    pit8254_init();

    printk("Initializing APIC\n");

    apic_init(ap);

    // We know the machine topology now...

    thread_set_cpu_count(apic_cpu_count());

    // Initialize APIC
    if (!apic_enable()) {
        // Fall back to 8259, APIC init failed
        printk("Enabling 8259 PIC\n");
        pic8259_enable();
    } else if (acpi_have8259pic()) {
        // 8259 present, mask them
        printk("Disabling 8259 PIC\n");
        pic8259_disable();
    } else {
        panic("No IOAPICs, no MPS, and no 8259! Can't use IRQs! Halting.");
    }

    printk("Initializing APIC timer\n");

    apic_init_timer();

    size_t cpu_nr = !ap ? 0 : thread_current_cpu(-1);

    if (!ap) {
        if (bootinfo_parameter(bootparam_t::smp_enable)) {
            printk("Starting SMP\n");
            apic_start_smp();
            gdt_init_tss(cpu_count);
        } else {
            printk("Starting with single processor\n");
            cpu_count = 1;
            gdt_init_tss(1);
        }

        gdt_load_tr(0);
    } else {
        gdt_load_tr(cpu_nr);
    }

    thread_init_cpu_count(cpu_count);

    callout_call(callout_type_t::smp_online);

#ifdef _CALL_TRACE_ENABLED
    extern void eainst_set_cpu_count(int count);
    eainst_set_cpu_count(thread_cpu_count());
#endif

    printk("Initializing RTC\n");

    cmos_init();

    printk("Enabling 8254 PIT\n");

    pit8254_enable();

    //printk("Enabling IRQs\n");
}

void cpu_patch_insn(void *addr, uint64_t value, size_t size)
{
    return cpu_patch_code(addr, &value, size);
}

void cpu_patch_code(void *addr, void const *src, size_t size)
{
    // Disable write protect
    cpu_cr0_change_bits(CPU_CR0_WP, 0);

    // Patch
    memcpy(addr, src, size);

    // Enable write protect
    cpu_cr0_change_bits(0, CPU_CR0_WP);
}

// points refers to an array of pointers to labels that are after the calls
// pass nullptr call_target to NOP out call instructions
void cpu_patch_calls(void const *call_target,
                     size_t point_count, uint32_t **points)
{
    for (size_t i = 0; i < point_count; ++i) {
        int32_t *point = (int32_t*)points[i];
        intptr_t dist = intptr_t(call_target) - intptr_t(point);
        if (call_target != nullptr) {
            assert(dist >= ext::numeric_limits<int32_t>::min() &&
                   dist <= ext::numeric_limits<int32_t>::max());
            cpu_patch_insn(point - 1, dist, sizeof(uint32_t));
        } else {
            // 0xE8 is call disp32 opcode
            uint8_t *call = (uint8_t*)(point - 1) - 1;
            assert(*call == 0xE8);
            if (*call == 0xE8)
                cpu_patch_nop(call, 5);
        }
    }
    atomic_fence();
}

// Fill a region with optimal nops
void cpu_patch_nop(void *addr, size_t size)
{
    static uint8_t const nop_insns[] = {
        0x90,
        0x66, 0x90,
        0x0F, 0x1F, 0x00,
        0x0F, 0x1F, 0x40, 0x00,
        0x0F, 0x1F, 0x44, 0x00, 0x00,
        0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00,
        0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00,
        0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    static uint8_t const nop_lookup[] = {
        0,
        0+1,
        0+2+1,
        0+3+2+1,
        0+4+3+2+1,
        0+5+4+3+2+1,
        0+6+5+4+3+2+1,
        0+7+6+5+4+3+2+1
    };

//    size_t line_remain = 64 - (uintptr_t(addr) & ~-64);

//    // If the range crosses a cache line boundary, split it into two nops
//    if (line_remain < size) {
//        cpu_patch_nop(addr, line_remain);
//        return cpu_patch_nop((char*)addr + line_remain, size - line_remain);
//    }

    uint8_t *out = (uint8_t*)addr;

    cpu_scoped_irq_disable irq_dis;

    while (size) {
        size_t insn_sz = size <= 15 ? size : 15;
        size -= insn_sz;

        // Place a number of 0x66 prefixes for sizes over 8 bytes
        if (insn_sz > 8) {
            out = ext::fill_n(out, insn_sz - 8, data16_prefix[0]);
            insn_sz = 8;
        }

        cpu_patch_code(out, nop_insns + nop_lookup[insn_sz - 1], insn_sz);

        out += insn_sz;
    }

    // Serializing instruction
    cpu_fault_address_set(0);
}

bool cpu_patch_jmp(void *addr, size_t size, void const *jmp_target)
{
    assert(size >= 5);
    uintptr_t code = uintptr_t(addr);
    uintptr_t next = code + 4;
    uintptr_t dest = uintptr_t(jmp_target);
    intptr_t dist = dest - next;
    if (likely(dist >= ext::numeric_limits<int32_t>::min() ||
               dist <= ext::numeric_limits<int32_t>::max())) {
        uint8_t buf[sizeof(jmp_disp32_insn)];
        memcpy(buf, jmp_disp32_insn, sizeof(buf));
        int32_t disp32 = dist;
        memcpy(buf + 1, &disp32, sizeof(disp32));
        cpu_patch_code(addr, jmp_disp32_insn, sizeof(jmp_disp32_insn));
        return true;
    }
    return false;
}

extern "C" int nofault_wrmsr(uint32_t msr, uint64_t value);

bool cpu_msr_set_safe(uint32_t msr, uint64_t value)
{
    return nofault_wrmsr(msr, value) == 0;
}

struct value_status_pair_t {
    uint64_t value;
    int64_t status;
};

extern "C" value_status_pair_t nofault_rdmsr(uint32_t msr);

bool cpu_msr_get_safe(uint32_t msr, uint64_t &value)
{
    auto result = nofault_rdmsr(msr);

    value = result.value;
    return result.status == 0;
}

// Runs once on each CPU at early boot
static void cpu_init_late_msrs_one_cpu()
{
    // Enable lfence speculation control on AMD processors
    // Enable lfence to block op dispatch until it retires
    // MSR is available on family 10h/12h/14h/15h/16h/17h
    if (cpuid_is_amd() && cpuid_family() >= 0x10 && cpuid_family() <= 0x17 &&
            cpuid_family() != 0x11 && cpuid_family() != 0x13) {
        uint64_t old_spec_ctl = 0;
        if (cpu_msr_get_safe(0xC0011029, old_spec_ctl)) {
            if (!(old_spec_ctl & 2)) {
                if (cpu_msr_set_safe(0xC0011029, old_spec_ctl | 2)) {
                    printdbg("Enabled lfence speculation control\n");
                } else {
                    printdbg("Unable to set MSR 0xC0011029,"
                             " lfence configuration, was %#" PRIx64 "\n",
                             old_spec_ctl);
                }
            } else {
                printdbg("lfence speculation control was already enabled\n");
            }
        } else {
            printdbg("Unable to read lfence speculation control MSR\n");
        }
    }
}

void cpu_init_late_msrs()
{
    workq::enqueue_on_all_barrier([=] (int cpu) {
        cpu_init_late_msrs_one_cpu();
    });
}

KERNEL_API bool arch_irq_disable()
{
    return cpu_irq_save_disable();
}

KERNEL_API void arch_irq_enable()
{
    cpu_irq_enable();
}

KERNEL_API void arch_irq_toggle(bool en)
{
    cpu_irq_toggle(en);
}

KERNEL_API void arch_poweroff()
{
    cpu_pv_qemu_x86_poweroff();
}

KERNEL_API void arch_ungraceful_stop()
{
    cpu_triple_fault();
}

void arch_cache_clean(void *vaddr, size_t size)
{
    if (!cpuid_has_clflush())
        return;

    for (size = (size + 63) & 63; size; (vaddr = (char*)vaddr + 64), size -= 64)
        cpu_clflush(vaddr);
}

void arch_cache_invalidate(void *vaddr, size_t size)
{

}

void arch_cache_clean_invalidate(void *vaddr, size_t size)
{

}
