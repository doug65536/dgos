#include "idt.h"
#include "isr.h"
#include "gdt.h"
#include "conio.h"
#include "printk.h"
#include "cpu/halt.h"
#include "time.h"
#include "interrupts.h"
#include "control_regs.h"
#include "string.h"

idt_entry_64_t idt[0x80];

void *(*irq_dispatcher)(int irq, isr_minimal_context_t *ctx);

cpu_flag_info_t const cpu_eflags_info[] = {
    { "ID",   EFLAGS_ID_BIT,   1, 0 },
    { "VIP",  EFLAGS_VIP_BIT,  1, 0 },
    { "VIF",  EFLAGS_VIF_BIT,  1, 0 },
    { "AC",   EFLAGS_AC_BIT,   1, 0 },
    { "VM",   EFLAGS_VM_BIT,   1, 0 },
    { "RF",   EFLAGS_RF_BIT,   1, 0 },
    { "NT",   EFLAGS_NT_BIT,   1, 0 },
    { "IOPL", EFLAGS_IOPL_BIT, EFLAGS_IOPL_MASK, 0 },
    { "OF",   EFLAGS_OF_BIT,   1, 0 },
    { "DF",   EFLAGS_DF_BIT,   1, 0 },
    { "IF",   EFLAGS_IF_BIT,   1, 0 },
    { "TF",   EFLAGS_TF_BIT,   1, 0 },
    { "SF",   EFLAGS_SF_BIT,   1, 0 },
    { "ZF",   EFLAGS_ZF_BIT,   1, 0 },
    { "AF",   EFLAGS_AF_BIT,   1, 0 },
    { "PF",   EFLAGS_PF_BIT,   1, 0 },
    { "CF",   EFLAGS_CF_BIT,   1, 0 },
    { 0,      -1,             -1, 0 }
};

char const *cpu_mxcsr_rc[] = {
    "Nearest",
    "Down",
    "Up",
    "Truncate"
};

cpu_flag_info_t const cpu_mxcsr_info[] = {
    { "IE",     MXCSR_IE_BIT, 1, 0 },
    { "DE",     MXCSR_DE_BIT, 1, 0 },
    { "ZE",     MXCSR_ZE_BIT, 1, 0 },
    { "OE",     MXCSR_OE_BIT, 1, 0 },
    { "UE",     MXCSR_UE_BIT, 1, 0 },
    { "PE",     MXCSR_PE_BIT, 1, 0 },
    { "DAZ",    MXCSR_DAZ_BIT, 1, 0 },
    { "IM",     MXCSR_IM_BIT, 1, 0 },
    { "DM",     MXCSR_DM_BIT, 1, 0 },
    { "ZM",     MXCSR_ZM_BIT, 1, 0 },
    { "OM",     MXCSR_OM_BIT, 1, 0 },
    { "UM",     MXCSR_UM_BIT, 1, 0 },
    { "PM",     MXCSR_PM_BIT, 1, 0 },
    { "RC",     MXCSR_RC_BIT, MXCSR_RC_BITS, cpu_mxcsr_rc },
    { "FZ",     MXCSR_FZ_BIT, 1, 0 }
};

typedef void (*isr_entry_t)(void);

const isr_entry_t isr_entry_points[73] = {
    isr_entry_0,  isr_entry_1,  isr_entry_2,  isr_entry_3,
    isr_entry_4,  isr_entry_5,  isr_entry_6,  isr_entry_7,
    isr_entry_8,  isr_entry_9,  isr_entry_10, isr_entry_11,
    isr_entry_12, isr_entry_13, isr_entry_14, isr_entry_15,
    isr_entry_16, isr_entry_17, isr_entry_18, isr_entry_19,
    isr_entry_20, isr_entry_21, isr_entry_22, isr_entry_23,
    isr_entry_24, isr_entry_25, isr_entry_26, isr_entry_27,
    isr_entry_28, isr_entry_29, isr_entry_30, isr_entry_31,

    isr_entry_32, isr_entry_33, isr_entry_34, isr_entry_35,
    isr_entry_36, isr_entry_37, isr_entry_38, isr_entry_39,
    isr_entry_40, isr_entry_41, isr_entry_42, isr_entry_43,
    isr_entry_44, isr_entry_45, isr_entry_46, isr_entry_47,

    isr_entry_48, isr_entry_49, isr_entry_50, isr_entry_51,
    isr_entry_52, isr_entry_53, isr_entry_54, isr_entry_55,
    isr_entry_56, isr_entry_57, isr_entry_58, isr_entry_59,
    isr_entry_60, isr_entry_61, isr_entry_62, isr_entry_63,
    isr_entry_64, isr_entry_65, isr_entry_66, isr_entry_67,
    isr_entry_68, isr_entry_69, isr_entry_70, isr_entry_71,

    isr_entry_72
};

extern void isr_entry_0xC0(void);

static void load_idtr(table_register_64_t *table_reg)
{
    __asm__ __volatile__ (
        "lidtq %[table_reg]\n\t"
        :
        : [table_reg] "m" (*table_reg)
    );
}

int idt_init(void)
{
    uint64_t addr;

    for (size_t i = 0; i < countof(isr_entry_points); ++i) {
        addr = (uint64_t)isr_entry_points[i];
        idt[i].offset_lo = (uint16_t)(addr & 0xFFFF);
        idt[i].offset_hi = (uint16_t)((addr >> 16) & 0xFFFF);
        idt[i].offset_64_31 = (uint16_t)((addr >> 32) & 0xFFFFFFFF);

        idt[i].type_attr = IDT_PRESENT |
                (i < 32 ? IDT_TRAP : IDT_INTR);

        idt[i].selector = IDT_SEL;
    }

    table_register_64_t idtr;

    addr = (uint64_t)idt;
    idtr.base_lo = (uint16_t)(addr & 0xFFFF);
    idtr.base_hi = (uint16_t)((addr >> 16) & 0xFFFF);
    idtr.base_hi1 = (uint16_t)((addr >> 32) & 0xFFFF);
    idtr.base_hi2 = (uint16_t)((addr >> 48) & 0xFFFF);
    idtr.limit = sizeof(idt) - 1;

    load_idtr(&idtr);

    return 0;
}

size_t cpu_format_flags_register(
        char *buf, size_t buf_size,
        uint64_t flags, cpu_flag_info_t const *info)
{
    size_t total_written = 0;
    int chars_needed;

    for (cpu_flag_info_t const *fi = info;
         fi->name; ++fi) {
        uint64_t value = (flags >> fi->bit) & fi->mask;

        if (value != 0 ||
                (fi->value_names && fi->value_names[0])) {
            char const *prefix = total_written > 0 ? " " : "";
            if (fi->value_names && fi->value_names[value]) {
                // Text value
                chars_needed = snprintf(
                            buf + total_written,
                            buf_size - total_written,
                            "%s%s=%s", prefix, fi->name,
                            fi->value_names[value]);
            } else if (fi->mask == 1) {
                // Single bit flag
                chars_needed = snprintf(
                            buf + total_written,
                            buf_size - total_written,
                            "%s%s", prefix, fi->name);
            } else {
                // Multi-bit flag
                chars_needed = snprintf(
                            buf + total_written,
                            buf_size - total_written,
                            "%s%s=%lX", prefix, fi->name, value);
            }
            if (chars_needed + total_written >= buf_size) {
                if (buf_size > 3) {
                    // Truncate with ellipsis
                    strcpy(buf + buf_size - 4, "...");
                    return buf_size - 1;
                } else if (buf_size > 0) {
                    // Truncate with * fill
                    memset(buf, '*', buf_size-1);
                    buf[buf_size-1] = 0;
                    return buf_size - 1;
                }
                // Wow, no room for anything
                return 0;
            }

            total_written += chars_needed;
        }
    }

    return total_written;
}

size_t cpu_describe_eflags(char *buf, size_t buf_size, uint64_t rflags)
{
    return cpu_format_flags_register(buf, buf_size, rflags,
                                     cpu_eflags_info);
}

size_t cpu_describe_mxcsr(char *buf, size_t buf_size, uint64_t mxcsr)
{
    return cpu_format_flags_register(buf, buf_size, mxcsr,
                                     cpu_mxcsr_info);

}

static void *unhandled_exception_handler(isr_full_context_t *ctx)
{
    char fmt_buf[64];
    int color = 0x0F;
    int width;
    static char const *reg_names[] = {
        "rax",
        "rbx",
        "rcx",
        "rdx",
        "rsi",
        "rdi",
        "rbp",
        " r8",
        " r9",
        "r10",
        "r11",
        "r12",
        "r13",
        "r14",
        "r15"
    };

    static char const *seg_names[] = {
        "ds", "es", "fs", "gs"
    };

    static char const reserved_exception[] = "Reserved";

    static char const * const exception_names[] = {
        "#DE Divide Error",
        "#DB Debug",
        "NMI",
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

    for (int i = 0; i < 16; ++i) {
        if (i < 15) {
            // General register name
            con_draw_xy(0, i, reg_names[i], color);
            // General register value
            snprintf(fmt_buf, sizeof(fmt_buf), "=%016lx ", ctx->gpr->r[i]);
            con_draw_xy(3, i, fmt_buf, color);
        }

        // XMM register name
        snprintf(fmt_buf, sizeof(fmt_buf), " %sxmm%d",
                 i < 10 ? " " : "",
                 i);
        con_draw_xy(29, i, fmt_buf, color);
        // XMM register value
        snprintf(fmt_buf, sizeof(fmt_buf), "=%016lx%016lx ",
                ctx->fpr->xmm[i].qword[0],
                ctx->fpr->xmm[i].qword[1]);
        con_draw_xy(35, i, fmt_buf, color);
    }

    for (int i = 0; i < 4; ++i) {
        // Segment register name
        con_draw_xy(37+i*8, 18, seg_names[i], color);
        // Segment register value
        snprintf(fmt_buf, sizeof(fmt_buf), "=%04x ",
                 ctx->gpr->s[i]);
        con_draw_xy(39+i*8, 18, fmt_buf, color);
    }

    // rsp
    con_draw_xy(0, 15, "ss:rsp", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%04lx:%012lx ",
             ctx->gpr->iret.ss, ctx->gpr->iret.rsp);
    con_draw_xy(6, 15, fmt_buf, color);

    // cs:rip
    con_draw_xy(
                                   0, 16, "cs:rip", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%04lx:%012lx",
             ctx->gpr->iret.cs, (uint64_t)ctx->gpr->iret.rip);
    con_draw_xy(6, 16, fmt_buf, color);

    // exception
    con_draw_xy(
                                   0, 17, "Exception", color);
    snprintf(fmt_buf, sizeof(fmt_buf), " 0x%02lx %s",
             ctx->gpr->info.interrupt,
             exception_names[ctx->gpr->info.interrupt]);
    con_draw_xy(9, 17, fmt_buf, color);

    // MXCSR
    width = snprintf(fmt_buf, sizeof(fmt_buf), "=%04x",
                     ctx->fpr->mxcsr);
    con_draw_xy(63-width, 16, "mxcsr", color);
    con_draw_xy(68-width, 16, fmt_buf, color);

    // MXCSR description
    width = cpu_describe_mxcsr(fmt_buf, sizeof(fmt_buf),
                       ctx->fpr->mxcsr);
    con_draw_xy(68-width, 17, fmt_buf, color);

    // fault address
    con_draw_xy(48, 19, "cr2", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%012lx",
             cpu_get_fault_address());
    con_draw_xy(51, 19, fmt_buf, color);

    // error code
    con_draw_xy(0, 18, "Error code", color);
    snprintf(fmt_buf, sizeof(fmt_buf), " 0x%012lx",
             ctx->gpr->info.error_code);
    con_draw_xy(10, 18, fmt_buf, color);

    // rflags (it's actually only 22 bits)
    con_draw_xy(0, 19, "rflags", color);
    width = snprintf(fmt_buf, sizeof(fmt_buf), "=%06lx ",
             ctx->gpr->iret.rflags);
    con_draw_xy(6, 19, fmt_buf, color);

    // rflags description
    cpu_describe_eflags(fmt_buf, sizeof(fmt_buf),
                       ctx->gpr->iret.rflags);
    con_draw_xy(6+width, 19, fmt_buf, color);

    // fsbase
    con_draw_xy(0, 20, "fsbase", color);
    width = snprintf(fmt_buf, sizeof(fmt_buf), "=%12lx ",
             (uint64_t)ctx->gpr->fsbase);
    con_draw_xy(6, 20, fmt_buf, color);

    halt_forever();

    return ctx;
}

isr_full_context_t *exception_isr_handler(isr_full_context_t *ctx)
{
    // FIXME: handle some exceptions like page faults sometime

    unhandled_exception_handler(ctx);
    return ctx;
}

void *isr_handler(isr_minimal_context_t *ctx)
{
    if (ctx->gpr->info.interrupt >= 32 &&
               ctx->gpr->info.interrupt < 48) {
        //
        // PIC IRQ
        ctx = irq_dispatcher(
                    ctx->gpr->info.interrupt - 32,
                    ctx);
    } else if (ctx->gpr->info.interrupt >= 48 &&
               ctx->gpr->info.interrupt <= 72) {
        //
        // APIC IRQ (and forced context switch)
        ctx = irq_dispatcher(
                    ctx->gpr->info.interrupt - 32,
                    ctx);
    } else {
    }

    return ctx;
}
