#include "idt.h"
#include "debug.h"
#include "assert.h"
#include "isr.h"
#include "irq.h"
#include "gdt.h"
#include "cpu.h"
#include "cpu/cpuid.h"
#include "conio.h"
#include "printk.h"
#include "halt.h"
#include "time.h"
#include "interrupts.h"
#include "string.h"
#include "except.h"
#include "mm.h"
//#include "asm_constants.h"
#include "thread_impl.h"
#include "stacktrace.h"
#include "apic.h"
#include "elf64.h"
#include "perf.h"
#include "utility.h"
#include "legacy_pic.h"

// Enforce that we use the correct value in syscall.S
C_ASSERT(SYSCALL_RFLAGS == (CPU_EFLAGS_IF | 2));

// Enforce that we don't try to set any flags bits that
// will be cleared anyway on sysret
C_ASSERT((SYSCALL_RFLAGS & ~uintptr_t(0x3C7FD7)) == 0);

irq_dispatcher_handler_t isr_lookup[INTR_COUNT];

idt_entry_64_t idt[INTR_COUNT];

ext::pair<void *, void *> ist_stacks[MAX_CPUS][IDT_IST_SLOT_COUNT];

static idt_unhandled_exception_handler_t unhandled_exception_handler_vec;

uint32_t xsave_supported_states;
uint32_t xsave_enabled_states;

static format_flag_info_t const cpu_eflags_info[] = {
    { "ID",   1,                    nullptr, CPU_EFLAGS_ID_BIT     },
    { "1",    1,                    nullptr, CPU_EFLAGS_ALWAYS_BIT },
    { "VIP",  1,                    nullptr, CPU_EFLAGS_VIP_BIT    },
    { "VIF",  1,                    nullptr, CPU_EFLAGS_VIF_BIT    },
    { "AC",   1,                    nullptr, CPU_EFLAGS_AC_BIT     },
    { "VM",   1,                    nullptr, CPU_EFLAGS_VM_BIT     },
    { "RF",   1,                    nullptr, CPU_EFLAGS_RF_BIT     },
    { "NT",   1,                    nullptr, CPU_EFLAGS_NT_BIT     },
    { "IOPL", CPU_EFLAGS_IOPL_MASK, nullptr, CPU_EFLAGS_IOPL_BIT   },
    { "OF",   1,                    nullptr, CPU_EFLAGS_OF_BIT     },
    { "DF",   1,                    nullptr, CPU_EFLAGS_DF_BIT     },
    { "IF",   1,                    nullptr, CPU_EFLAGS_IF_BIT     },
    { "TF",   1,                    nullptr, CPU_EFLAGS_TF_BIT     },
    { "SF",   1,                    nullptr, CPU_EFLAGS_SF_BIT     },
    { "ZF",   1,                    nullptr, CPU_EFLAGS_ZF_BIT     },
    { "AF",   1,                    nullptr, CPU_EFLAGS_AF_BIT     },
    { "PF",   1,                    nullptr, CPU_EFLAGS_PF_BIT     },
    { "CF",   1,                    nullptr, CPU_EFLAGS_CF_BIT     },
    { nullptr,0,                    nullptr, -1                    }
};

static char const *cpu_mxcsr_rc[] = {
    "Nearest",
    "Down",
    "Up",
    "Truncate"
};

static format_flag_info_t const cpu_mxcsr_info[] = {
    { "IE",     1,                 nullptr,      CPU_MXCSR_IE_BIT  },
    { "DE",     1,                 nullptr,      CPU_MXCSR_DE_BIT  },
    { "ZE",     1,                 nullptr,      CPU_MXCSR_ZE_BIT  },
    { "OE",     1,                 nullptr,      CPU_MXCSR_OE_BIT  },
    { "UE",     1,                 nullptr,      CPU_MXCSR_UE_BIT  },
    { "PE",     1,                 nullptr,      CPU_MXCSR_PE_BIT  },
    { "DAZ",    1,                 nullptr,      CPU_MXCSR_DAZ_BIT },
    { "IM",     1,                 nullptr,      CPU_MXCSR_IM_BIT  },
    { "DM",     1,                 nullptr,      CPU_MXCSR_DM_BIT  },
    { "ZM",     1,                 nullptr,      CPU_MXCSR_ZM_BIT  },
    { "OM",     1,                 nullptr,      CPU_MXCSR_OM_BIT  },
    { "UM",     1,                 nullptr,      CPU_MXCSR_UM_BIT  },
    { "PM",     1,                 nullptr,      CPU_MXCSR_PM_BIT  },
    { "RC",     CPU_MXCSR_RC_BITS, cpu_mxcsr_rc, CPU_MXCSR_RC_BIT  },
    { "FZ",     1,                 nullptr,      CPU_MXCSR_FZ_BIT  },
    { nullptr,  0,                 nullptr,      -1                }
};

static char const *cpu_fpucw_pc[] = {
    "24-bit",
    "??",
    "53-bit",
    "64-bit"
};

static format_flag_info_t const cpu_fpucw_info[] = {
    { "IM",     1,                  nullptr,      CPU_FPUCW_IM_BIT },
    { "DM",     1,                  nullptr,      CPU_FPUCW_DM_BIT },
    { "ZM",     1,                  nullptr,      CPU_FPUCW_ZM_BIT },
    { "OM",     1,                  nullptr,      CPU_FPUCW_OM_BIT },
    { "UM",     1,                  nullptr,      CPU_FPUCW_UM_BIT },
    { "PM",     1,                  nullptr,      CPU_FPUCW_PM_BIT },
    { "PC",     CPU_FPUCW_PC_BITS,  cpu_fpucw_pc, CPU_FPUCW_PC_BIT },
    // This looks like a bug but no, it is sharing the mxcsr lookup table:
    { "RC",     CPU_FPUCW_RC_BITS,  cpu_mxcsr_rc, CPU_FPUCW_RC_BIT },
    { "1",      1,                  nullptr,      CPU_FPUCW_ALWAYS_BIT },
    { nullptr,  0,                  nullptr,      -1               }
};

static format_flag_info_t const cpu_fpusw_info[] = {
    { "IE",    1,                   nullptr, CPU_FPUSW_IE_BIT  },
    { "DE",    1,                   nullptr, CPU_FPUSW_DE_BIT  },
    { "ZE",    1,                   nullptr, CPU_FPUSW_ZE_BIT  },
    { "OE",    1,                   nullptr, CPU_FPUSW_OE_BIT  },
    { "UE",    1,                   nullptr, CPU_FPUSW_UE_BIT  },
    { "PE",    1,                   nullptr, CPU_FPUSW_PE_BIT  },
    { "SF",    1,                   nullptr, CPU_FPUSW_SF_BIT  },
    { "ES",    1,                   nullptr, CPU_FPUSW_ES_BIT  },
    { "C0(c)", 1,                   nullptr, CPU_FPUSW_C0_BIT  },
    { "C1",    1,                   nullptr, CPU_FPUSW_C1_BIT  },
    { "C2(p)", 1,                   nullptr, CPU_FPUSW_C2_BIT  },
    { "TOP",   CPU_FPUSW_TOP_BITS,  nullptr, CPU_FPUSW_TOP_BIT },
    { "C3(z)", 1,                   nullptr, CPU_FPUSW_C3_BIT  },
    { "B",     1,                   nullptr, CPU_FPUSW_B_BIT   },
    { nullptr, 0,                   nullptr, -1,               }
};

static char const reserved_exception[] = "Reserved";

static char const * const exception_names[] = {
    "#DE Divide Error",
    "#DB Debug",
    "#NM NMI",
    "#BP Breakpoint,",
    "#OF Overflow",
    "#BR BOUND Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    reserved_exception,
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack Fault",
    "#GP General Protection",
    "#PF Page Fault",
    reserved_exception,
    "#MF Floating-Point Error",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD",
    "#VE Virtualization",
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception
};

typedef void (*isr_entry_t)(void);

#if 0
static void idtr_load(table_register_64_t *table_reg)
{
    __asm__ __volatile__ (
        "lidtq (%[table_reg])\n\t"
        :
        : [table_reg] "r" (&table_reg->limit)
        : "memory"
    );
}
#endif

extern "C" isr_context_t *exception_isr_handler(int intr, isr_context_t *ctx);
extern "C" isr_context_t *cpu_gpf_handler(int intr, isr_context_t *ctx);

_constructor(ctor_cpu_init_cpus_done) static void isr_lookup_init()
{
    // Interrupt dispatch
    // 0x00-0x1F -> exception_isr_handler
    // 0x20-0x2F -> intr_invoke
    // 0x30-0xEF -> apic_dispatcher
    // 0xF0-0xFF -> pic_dispatcher

    for (size_t i = INTR_EX_BASE; i <= INTR_EX_LAST; ++i)
        isr_lookup[i] = exception_isr_handler;

    // Couple of special cases
    isr_lookup[INTR_EX_NMI] = intr_invoke;
    isr_lookup[INTR_EX_GPF] = cpu_gpf_handler;
    isr_lookup[INTR_EX_PAGE] = mmu_page_fault_handler;

    for (size_t i = INTR_SOFT_BASE; i <= INTR_SOFT_LAST; ++i)
        isr_lookup[i] = intr_invoke;

    for (size_t i = INTR_APIC_DSP_BASE; i <= INTR_APIC_DSP_LAST; ++i)
        isr_lookup[i] = apic_dispatcher;

    for (size_t i = INTR_PIC_DSP_BASE; i <= INTR_PIC_DSP_LAST; ++i)
        isr_lookup[i] = pic8259_dispatcher;

    for (size_t i = 0; i < 255; ++i)
        assert(isr_lookup[i] != intr_handler_t(nullptr));
}

// Assume the worst
xsave_support_t xsave_support = xsave_support_t::FXSAVE;

void idt_xsave_detect(int ap)
{
    cpu_scoped_irq_disable intr_was_enabled;

    // Patch FS/GS loading code if CPU supports wrfsbase/wrgsbase

    if (!ap)
        xsave_support = xsave_support_t::FXSAVE;

    while (cpuid_has_xsave()) {
        cpuid_t info;

        // Get size of save area
        bool cpuid_ok = cpuid(&info, CPUID_INFO_XSAVE, 0);
        assert(cpuid_ok);

        if (!ap) {
            xsave_support = xsave_support_t::XSAVE;

            // Store size of save area
            assert(info.ebx < UINT16_MAX);
            sse_context_size = (info.ebx + 15) & -16;
        }

        // Use compact format if available
        // xsave area must be aligned on a 64 byte boundary
        if (!ap)
            sse_context_size = (sse_context_size + 63) & -64;

        if (cpuid_has_xsaves()) {
            // xsaves available, awesome!

            if (!ap)
                xsave_support = xsave_support_t::XSAVES;

            cpu_msr_set(CPU_MSR_IA32_XSS, 0);
        } else if (cpuid_has_xsavec()) {
            // xsavec available

            if (!ap)
                xsave_support = xsave_support_t::XSAVEC;

        } else if (cpuid_has_xsaveopt()) {
            // xsaveopt available

            if (!ap)
                xsave_support = xsave_support_t::XSAVEOPT;

        }

        // Save offsets/sizes of extended contexts

        if (cpuid_has_avx() && cpuid(&info, CPUID_INFO_XSAVE, XCR0_AVX_BIT)) {
            assert(info.ebx < UINT16_MAX);
            assert(info.eax < UINT16_MAX);
            assert(info.ebx + info.eax <= UINT16_MAX);

            if (!ap) {
                sse_avx_offset = (uint16_t)info.ebx;
                sse_avx_size = (uint16_t)info.eax;
            }
        }

        if (cpuid_has_avx512f()) {
            if (cpuid(&info, CPUID_INFO_XSAVE, XCR0_AVX512_OPMASK_BIT)) {
                assert(info.ebx < UINT16_MAX);
                assert(info.eax < UINT16_MAX);
                assert(info.ebx + info.eax <= UINT16_MAX);

                if (!ap) {
                    sse_avx512_opmask_offset = (uint16_t)info.ebx;
                    sse_avx512_opmask_size = (uint16_t)info.eax;
                }
            }

            if (cpuid(&info, CPUID_INFO_XSAVE, XCR0_AVX512_UPPER_BIT)) {
                assert(info.ebx < UINT16_MAX);
                assert(info.eax < UINT16_MAX);
                assert(info.ebx + info.eax <= UINT16_MAX);

                if (!ap) {
                    sse_avx512_upper_offset = (uint16_t)info.ebx;
                    sse_avx512_upper_size = (uint16_t)info.eax;
                }
            }

            if (cpuid(&info, CPUID_INFO_XSAVE, XCR0_AVX512_XREGS_BIT)) {
                assert(info.ebx < UINT16_MAX);
                assert(info.eax < UINT16_MAX);
                assert(info.ebx + info.eax <= UINT16_MAX);

                if (!ap) {
                    sse_avx512_xregs_offset = (uint16_t)info.ebx;
                    sse_avx512_xregs_size = (uint16_t)info.eax;
                }
            }
        }

        // Sanity check offsets
        assert(sse_avx_offset +
               sse_avx_size <= sse_context_size);

        assert(sse_avx512_opmask_offset +
               sse_avx512_opmask_size <= sse_context_size);

        assert(sse_avx512_upper_offset +
               sse_avx512_upper_size <= sse_context_size);

        assert(sse_avx512_xregs_offset +
               sse_avx512_xregs_size <= sse_context_size);

        return;
    }

    // Execution reaches here if we must use the old fxsave instruction

    if (!ap)
        sse_context_size = 512;
}

void idt_ist_adjust(int cpu, size_t ist, ptrdiff_t adj)
{
    size_t cpu_count = thread_cpu_count();
    if (cpu >= 0 && size_t(cpu) < cpu_count) {
        tss_list[cpu].ist[ist] += adj;
        return;
    }

    for (size_t i = 0; i < cpu_count; ++i)
        tss_list[cpu].ist[ist] += adj;
}

//isr_context_t *debug_exception_handler(int intr, isr_context_t *ctx)
//{
//    assert(intr == INTR_EX_DEBUG);
//    //printdbg("Ignored debug exception\n");
//    return ctx;
//}

//isr_context_t *opcode_exception_handler(int intr, isr_context_t *ctx)
//{
//    return nullptr;
//}

extern char ___isr_st[];

uintptr_t isr_entry_point(size_t i)
{
    return uintptr_t(___isr_st + (i << 4));
}

static uint8_t idt_vector_type(size_t vec)
{
    switch (vec) {
    case INTR_EX_DIV:
    case INTR_EX_DEBUG:
    case INTR_EX_BREAKPOINT:
    case INTR_EX_OVF:
    case INTR_EX_BOUND:
    case INTR_EX_OPCODE:
    case INTR_EX_PAGE:
    case INTR_EX_ALIGNMENT:
    case INTR_EX_DEV_NOT_AV:
    case INTR_EX_COPR_SEG:
    case INTR_EX_SEGMENT:
    case INTR_EX_TSS:
    case INTR_EX_STACK:
    case INTR_EX_GPF:
    case INTR_EX_MATH:
    case INTR_EX_SIMD:
        // Don't mask IRQs
        return IDT_TRAP;

    case INTR_EX_MACHINE:
    case INTR_EX_VIRTUALIZE:
    case INTR_EX_NMI:
    case INTR_THREAD_YIELD:
    case INTR_EX_DBLFAULT:
    default:
        // Mask IRQs
        return IDT_INTR;
    }
}

int idt_init(int ap)
{
    uintptr_t addr;

    if (!ap) {
        for (size_t i = 0; i < 256; ++i) {
            addr = isr_entry_point(i);

            uint8_t first_opcode = 0;
            memcpy(&first_opcode, (void*)addr, sizeof(first_opcode));

            // Sanity check (tolerate software breakpoint)
            assert(first_opcode == 0x6a || first_opcode == 0xcc);

            idt[i].offset_lo = uint16_t(addr & 0xFFFF);
            idt[i].offset_hi = uint16_t((addr >> 16) & 0xFFFF);
            idt[i].offset_64_31 = uint32_t((addr >> 32) & 0xFFFFFFFF);

            idt[i].type_attr = IDT_PRESENT | idt_vector_type(i);

            idt[i].selector = IDT_SEL;
        }

        // Assign IST entries to interrupts
        idt[INTR_EX_STACK].ist = IDT_IST_SLOT_STACK;
        idt[INTR_EX_DBLFAULT].ist = IDT_IST_SLOT_DBLFAULT;
        idt[INTR_IPI_FL_TRACE].ist = IDT_IST_SLOT_FLUSH_TRACE;
        idt[INTR_EX_NMI].ist = IDT_IST_SLOT_NMI;
    }

    table_register_64_t idtr;

    addr = (uintptr_t)idt;
    idtr.base = addr;
    idtr.limit = sizeof(idt) - 1;

    cpu_idtr_set(idtr);

    return 0;
}

size_t cpu_describe_eflags(char *buf, size_t buf_size, uintptr_t rflags)
{
    return format_flags_register(buf, buf_size, rflags,
                                     cpu_eflags_info);
}

size_t cpu_describe_mxcsr(char *buf, size_t buf_size, uintptr_t mxcsr)
{
    return format_flags_register(buf, buf_size, mxcsr,
                                     cpu_mxcsr_info);

}

size_t cpu_describe_fpucw(char *buf, size_t buf_size, uint16_t fpucw)
{
    return format_flags_register(buf, buf_size, fpucw,
                                     cpu_fpucw_info);

}

size_t cpu_describe_fpusw(char *buf, size_t buf_size, uint16_t fpusw)
{
    return format_flags_register(buf, buf_size, fpusw,
                                     cpu_fpusw_info);

}

struct xsave_hdr_t {
    uint64_t xstate_bv;
    uint64_t xcomp_bv;
};

// NOTE: need separate get/set to handle modifying compact format in place
static uint64_t const *cpu_get_fpr_reg(isr_context_t *ctx, uint8_t reg)
{
    xsave_hdr_t const *hdr = (xsave_hdr_t*)((char*)ctx->fpr + 512);
    uint64_t const *area;
    bool compact = (hdr->xcomp_bv & (uint64_t(1) << 63)) != 0;
    int index = 0;
    bool bit;

    static uint64_t constexpr zero[8] = {};

    if (reg < 16) {
        // reg  0-15 xmm0-xmm15 (128 bits each)
        return ISR_CTX_SSE_XMMn_q_ptr(ctx, reg);
    } else if (sse_avx_offset && reg < 32) {
        // reg 16-31 ymm0h-ymm15h (128 bits each)
        reg -= 16;

        area = (uint64_t*)((char*)ctx->fpr + sse_avx_offset);

        if (compact) {
            // Compact format
            // ISDM Vol-1 13.4.3: Each state component i (i ≥ 2) is
            // located at a byte offset from the base address of the
            // XSAVE area based on the XCOMP_BV field in the XSAVE header:
            //  If XCOMP_BV[i] = 0, state component i is not in the XSAVE area
            //  If XCOMP_BV[i] = 1, state component i is located at a byte
            //  offset location I from the base address of the XSAVE area,
            //  where location I is determined by the following items:
            //  If align I = 0, location I = location J + size J. (This
            //  item implies that state component i is located immediately
            //  following the preceding state component whose bit is set in
            //  XCOMP_BV.)
            //  - If align I = 1,
            //     location I = ceiling(location J + size J , 64).
            //  (This item implies that state component i is located on
            //  the next 64-byte boundary following the preceding state
            //  component whose bit is set in XCOMP_BV.)
            //
            // States:
            //  0: x87
            //  1: SSE
            //  2: AVX
            //  3: BNDREGS
            //  4: BNDCSR
            //  5: OPMASK
            //  6: ZMMHI256
            //  7: ZMMHI16
            //  8: PT (not supported)
            //  9: PKRU (not supported)

            for (uint8_t i = 2; i < 16; ++i) {
                bit = hdr->xcomp_bv & (1 << (i + 2));
                index += bit;
            }

            if (!bit)
                return zero;

            return area + (index * 2);
        } else {
            // Standard format

            return area + (reg * 2);
        }
    } else if (sse_avx512_upper_offset && reg < 64) {
        // reg 32-47 zmm0h-zmm15h (256 bits each)
        reg -= 32;

    } else if (sse_avx512_xregs_offset && reg < 64) {
        // reg 48-63 zmm16-zmm31  (512 bits each)
        reg -= 48;

    } else if (sse_avx512_opmask_offset && reg < 72) {
        // reg 64-72 are opmask (64 bits each)
    }

    return nullptr;
}

//static void stack_trace(isr_context_t *ctx,
//                        void (*cb)(uintptr_t rbp, uintptr_t rip))
//{
//    uintptr_t *frame_ptr = (uintptr_t*)ISR_CTX_REG_RBP(ctx);

//    uintptr_t frame_rbp;
//    uintptr_t frame_rip;

//    do {
//        if (!mpresent(uintptr_t(frame_ptr), sizeof(uintptr_t) * 2))
//            return;

//        frame_rbp = frame_ptr[0];
//        frame_rip = frame_ptr[1];

//        cb(frame_rbp, frame_rip);

//        frame_ptr = (uintptr_t*)frame_rbp;
//    } while (frame_rbp);
//}

//static void dump_frame(uintptr_t rbp, uintptr_t rip)
//{
//    printk("at cfa=%#16zx ip=%#16zx\n", rbp, rip);
//}

// In pop order if you were restoring context
static char const *reg_names[] = {
    // Clobbered parameter registers below here
    "rdi",
    "rsi",
    "rdx",
    "rcx",
    " r8",
    " r9",
    // Clobbered registers below here
    "rax",
    "r10",
    "r11",
    // Call preserved below here
    "r12",
    "r13",
    "r14",
    "r15",
    "rbx",
    "rbp"
};

static char const *seg_names[] = {
    "ds", "es", "fs", "gs"
};

void dump_context(isr_context_t *ctx, int to_screen)
{
    char fmt_buf[64];
    int color = 0x0F;
    int width;

    void *fsbase = thread_get_fsbase(-1);
    void *gsbase = thread_get_gsbase(-1);

    //
    // Dump context to debug console

    printdbg("- Exception -------------------------------\n");

    // General registers (except rsp)
    for (int i = 0; i < 15; ++i) {
        printdbg("%s=%16lx\n",
                 reg_names[i],
                 ctx->gpr.r.r[i]);
    }

    bool has_fpu_ctx = ISR_CTX_FPU(ctx) != nullptr;

    if (sse_avx512_xregs_offset && sse_avx512_xregs_size) {
        // 32 register AVX-512

    } else if (sse_avx512_upper_offset && sse_avx512_upper_size) {
        // AVX-512

    } else if (sse_avx_offset && sse_avx_size) {
        // AVX
        for (int i = 0; has_fpu_ctx && i < 16; ++i) {
            uint64_t const *lo = cpu_get_fpr_reg(ctx, i);
            uint64_t const *hi = cpu_get_fpr_reg(ctx, i + 16);
            printdbg("%symm%d=%16lx:%16lx:%16lx:%16lx\n",
                     i > 9 ? "" : " ", i,
                     hi[1], hi[0], lo[1], lo[0]);
        }
    } else {
        // xmm registers
        for (int i = 0; has_fpu_ctx && i < 16; ++i) {
            printdbg("%sxmm%d=%16lx%16lx\n",
                     i > 9 ? "" : " ", i,
                     ISR_CTX_SSE_XMMn_q(ctx, i, 1),
                     ISR_CTX_SSE_XMMn_q(ctx, i, 0));
        }
    }

    // Segment registers
    for (int i = 0; i < 4; ++i) {
        printdbg("%s=%#.4x\n",
                 seg_names[i],
                 ctx->gpr.s.r[i]);
    }

    printdbg("cs:rip=%#4zx:%#016zx\n",
             ISR_CTX_REG_CS(ctx),
             uintptr_t((void*)ISR_CTX_REG_RIP(ctx)));

    printdbg("ss:rsp=%#4zx:%#016zx\n",
             ISR_CTX_REG_SS(ctx),
             ISR_CTX_REG_RSP(ctx));

    // Exception
    if (ISR_CTX_INTR(ctx) < 32) {
        printdbg("Exception %#02zx %s\n",
                 size_t(ISR_CTX_INTR(ctx)),
                 exception_names[ISR_CTX_INTR(ctx)]);
    } else {
        printdbg("Interrupt %#02zx\n", size_t(ISR_CTX_INTR(ctx)));
    }

    // mxcsr and description
    if (has_fpu_ctx) {
        cpu_describe_mxcsr(fmt_buf, sizeof(fmt_buf), ISR_CTX_SSE_MXCSR(ctx));
        printdbg("mxcsr=%#.4x %s\n", ISR_CTX_SSE_MXCSR(ctx), fmt_buf);

        // fpucw and description
        cpu_describe_fpucw(fmt_buf, sizeof(fmt_buf), ISR_CTX_FPU_FCW(ctx));
        printdbg("fpucw=%#.4x %s\n", ISR_CTX_FPU_FCW(ctx), fmt_buf);

        // fpusw and description
        cpu_describe_fpusw(fmt_buf, sizeof(fmt_buf), ISR_CTX_FPU_FSW(ctx));
        printdbg("fpusw=%#.4x %s\n", ISR_CTX_FPU_FSW(ctx), fmt_buf);
    }

    // fault address
    printdbg("cr3=%16lx\n", cpu_page_directory_get());

    // fault address
    printdbg("cr2=%16lx\n", cpu_fault_address_get());

    // error code
    printdbg("Error code %#4lx\n", ISR_CTX_ERRCODE(ctx));

    // rflags (it's actually only 22 bits) and description
    cpu_describe_eflags(fmt_buf, sizeof(fmt_buf), ISR_CTX_REG_RFLAGS(ctx));
    printdbg("rflags=%#.16lx %s\n", ISR_CTX_REG_RFLAGS(ctx), fmt_buf);

    // fsbase
    printdbg("fsbase=%#.16lx\n", uintptr_t(fsbase));

    // gsbase
    printdbg("gsbase=%#.16lx\n", uintptr_t(gsbase));

    ptrdiff_t rip = (ptrdiff_t)ISR_CTX_REG_RIP(ctx);

    module_t *closest_module = modload_closest(rip);

    if (closest_module) {
        printdbg("Closest module is %s, at +%#zx\n",
                 modload_get_name(closest_module).c_str(),
                 rip - modload_get_base_adj(closest_module));
    }

    //perf_stacktrace_decoded();

    //
    // Dump context to screen

    if (!to_screen)
        return;

    for (int i = 0; i < 16; ++i) {
        if (i < 15) {
            // General register name
            con_draw_xy(0, i, reg_names[i], color);
            // General register value
            snprintf(fmt_buf, sizeof(fmt_buf), "=%16lx ",
                     ISR_CTX_REG_GPR_n(ctx, i));
            con_draw_xy(3, i, fmt_buf, color);
        }

        if (has_fpu_ctx) {
            // XMM register name
            snprintf(fmt_buf, sizeof(fmt_buf), " %sxmm%d",
                     i < 10 ? " " : "",
                     i);
            con_draw_xy(29, i, fmt_buf, color);

            // XMM register value
            snprintf(fmt_buf, sizeof(fmt_buf), "=%16lx%16lx ",
                    ISR_CTX_SSE_XMMn_q(ctx, i, 1),
                    ISR_CTX_SSE_XMMn_q(ctx, i, 0));
            con_draw_xy(35, i, fmt_buf, color);
        }
    }

    for (int i = 0; i < 4; ++i) {
        // Segment register name
        con_draw_xy(37+i*8, 18, seg_names[i], color);
        // Segment register value
        snprintf(fmt_buf, sizeof(fmt_buf), "=%#.4x ",
                 ISR_CTX_REG_SEG_n(ctx, i));
        con_draw_xy(39+i*8, 18, fmt_buf, color);
    }

    // ss:rsp
    con_draw_xy(0, 15, "ss:rsp", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%04lx:%16lx ",
             ISR_CTX_REG_SS(ctx), ISR_CTX_REG_RSP(ctx));
    con_draw_xy(6, 15, fmt_buf, color);

    // cs:rip
    con_draw_xy(0, 16, "cs:rip", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%#04lx:%#16zx",
             ISR_CTX_REG_CS(ctx), (uintptr_t)ISR_CTX_REG_RIP(ctx));
    con_draw_xy(6, 16, fmt_buf, color);

    if (ISR_CTX_INTR(ctx) < 32) {
        // exception
        con_draw_xy(0, 17, "Exception", color);
        snprintf(fmt_buf, sizeof(fmt_buf), " %#02zx %s",
                 size_t(ISR_CTX_INTR(ctx)),
                 exception_names[ISR_CTX_INTR(ctx)]);
        con_draw_xy(9, 17, fmt_buf, color);
    } else {
        con_draw_xy(0, 17, "Interrupt", color);
        snprintf(fmt_buf, sizeof(fmt_buf), " %#02zx",
                 size_t(ISR_CTX_INTR(ctx)));
        con_draw_xy(9, 17, fmt_buf, color);
    }

    if (has_fpu_ctx) {
        // MXCSR
        width = snprintf(fmt_buf, sizeof(fmt_buf), "=%#.4x",
                         ISR_CTX_SSE_MXCSR(ctx));
        con_draw_xy(63-width, 16, "mxcsr", color);
        con_draw_xy(68-width, 16, fmt_buf, color);

        // MXCSR description
        width = cpu_describe_mxcsr(fmt_buf, sizeof(fmt_buf),
                           ISR_CTX_SSE_MXCSR(ctx));
        con_draw_xy(68-width, 17, fmt_buf, color);
    }

    // fault address
    con_draw_xy(48, 19, "cr2", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%16lx",
             cpu_fault_address_get());
    con_draw_xy(51, 19, fmt_buf, color);

    // error code
    con_draw_xy(0, 18, "Error code", color);
    snprintf(fmt_buf, sizeof(fmt_buf), " %#16lx",
             ISR_CTX_ERRCODE(ctx));
    con_draw_xy(10, 18, fmt_buf, color);

    // rflags (it's actually only 22 bits)
    con_draw_xy(0, 19, "rflags", color);
    width = snprintf(fmt_buf, sizeof(fmt_buf), "=%06lx ",
             ISR_CTX_REG_RFLAGS(ctx));
    con_draw_xy(6, 19, fmt_buf, color);

    // rflags description
    cpu_describe_eflags(fmt_buf, sizeof(fmt_buf),
                       ISR_CTX_REG_RFLAGS(ctx));
    con_draw_xy(6+width, 19, fmt_buf, color);

    // fsbase
    con_draw_xy(0, 20, "fsbase", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%#.16lx ", uintptr_t(fsbase));
    con_draw_xy(6, 20, fmt_buf, color);

    // gsbase
    con_draw_xy(0, 21, "gsbase", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%#.16lx ", uintptr_t(gsbase));
    con_draw_xy(6, 21, fmt_buf, color);

    if (ISR_CTX_INTR(ctx) == INTR_EX_GPF)
        apic_dump_regs(0);

    //stacktrace(dump_frame);
}

isr_context_t *unhandled_exception_handler(isr_context_t *ctx)
{
    cpu_debug_break();

    if (unhandled_exception_handler_vec) {
        isr_context_t *handled_ctx = unhandled_exception_handler_vec(
                    ISR_CTX_INTR(ctx), ctx);

        if (handled_ctx)
            return handled_ctx;
    }

    char const *name = ISR_CTX_INTR(ctx) < countof(exception_names)
            ? exception_names[ISR_CTX_INTR(ctx)]
            : nullptr;

    if (ISR_CTX_INTR(ctx) == INTR_EX_OPCODE) {
        uint8_t const *chk = (uint8_t const *)ISR_CTX_REG_RIP(ctx);
        printk("Opcode = %#.2x\n", *chk);
    }

    printk("\nUnhandled exception %#zx (%s) at RIP=%p\n",
           size_t(ISR_CTX_INTR(ctx)),
           name ? name : "??", (void*)ISR_CTX_REG_RIP(ctx));

    dump_context(ctx, 1);
    halt_forever();
    return ctx;
}

void idt_set_unhandled_exception_handler(
        idt_unhandled_exception_handler_t handler)
{
    assert(!unhandled_exception_handler_vec);
    unhandled_exception_handler_vec = handler;
}

void idt_set_ist_stack(size_t cpu_nr, size_t ist_slot, void *st, void *en)
{
    assert(cpu_nr < MAX_CPUS);
    assert(ist_slot < IDT_IST_SLOT_COUNT);
    ist_stacks[cpu_nr][ist_slot] = { st, en };
}

ext::pair<void *, void *> idt_get_ist_stack(size_t cpu_nr, size_t ist_slot)
{
    assert(cpu_nr < MAX_CPUS);
    assert(ist_slot < IDT_IST_SLOT_COUNT);

    if (ist_slot == 0)
        return { nullptr, nullptr };

    return ist_stacks[cpu_nr][ist_slot];
}

void idt_override_vector(int intr, irq_dispatcher_handler_t handler)
{
    idt[intr].offset_lo = uint16_t(uintptr_t(handler) >> 0);
    idt[intr].offset_hi = uint16_t(uintptr_t(handler) >> 16);
    idt[intr].offset_64_31 = uintptr_t(handler) >> 32;
}

void idt_clone_debug_exception_dispatcher(void)
{
    // From linker script
    extern char ___isr_st[];
    extern char ___isr_en[];
    char const * bp_entry = (char const *)isr_entry_point(INTR_EX_BREAKPOINT);
    char const * debug_entry = (char const *)isr_entry_point(INTR_EX_DEBUG);
    char const * nmi_entry = (char const *)isr_entry_point(INTR_EX_NMI);

    size_t isr_size = ___isr_en - ___isr_st;
    size_t bp_entry_ofs = bp_entry - ___isr_st;
    size_t debug_entry_ofs = debug_entry - ___isr_st;
    size_t nmi_entry_ofs = nmi_entry - ___isr_st;

    char *clone = (char*)mmap(nullptr, isr_size,
                              PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_POPULATE);
    if (unlikely(clone == MAP_FAILED))
        panic_oom();

    memcpy(clone, ___isr_st, isr_size);

    auto clone_breakpoint = irq_dispatcher_handler_t(clone + bp_entry_ofs);
    auto clone_debug = irq_dispatcher_handler_t(clone + debug_entry_ofs);
    auto clone_nmi = irq_dispatcher_handler_t(clone + nmi_entry_ofs);

    idt_override_vector(INTR_EX_BREAKPOINT, clone_breakpoint);
    idt_override_vector(INTR_EX_DEBUG, clone_debug);
    idt_override_vector(INTR_EX_NMI, clone_nmi);
}

extern char const isr_entry_st[];
extern char const isr_common[];

bool idt_clone_isr_entry(void *dest)
{
    size_t const sz = (char*)isr_common - (char*)isr_entry_st;
    size_t const dist = (char*)dest - (char*)isr_entry_st;

    // k
    memcpy(dest, isr_entry_st, sz);

    // Make it executable
    mprotect(dest, sz, PROT_READ | PROT_EXEC);

    // Update IDT
    for (size_t i = 0; i < 256; ++i) {
        idt_override_vector(i, irq_dispatcher_handler_t(
                                (char*)isr_entry_point(i) + dist));
    }

    return true;
}

void idt_mitigate_meltdown(void*)
{
    if (!cpuid_has_bug_meltdown())
        return;

    printk("CPU meltdown vulnerability is unmitigated\n");
}

REGISTER_CALLOUT(idt_mitigate_meltdown, nullptr,
                 callout_type_t::vmm_ready, "000");
