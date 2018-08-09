
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
#include "cpuid.h"
#include "callout.h"
#include "printk.h"
#include "idt.h"
#include "interrupts.h"
#include "syscall.h"
#include "string.h"

uint32_t default_mxcsr_mask;

void cpu_init_early(int ap)
{
    if (ap)
        mm_set_master_pagedir();

    gdt_init(ap);
    gdt_init_tss_early();
    idt_init(ap);

    cpu_cs_set(GDT_SEL_KERNEL_CODE64);
    cpu_ss_set(GDT_SEL_KERNEL_DATA);
    cpu_ds_set(GDT_SEL_USER_DATA | 3);
    cpu_es_set(GDT_SEL_USER_DATA | 3);
    cpu_fs_set(GDT_SEL_USER_DATA | 3);
    cpu_gs_set(GDT_SEL_USER_DATA | 3);

    // Enable SSE early
    cpu_cr4_change_bits(0, CPU_CR4_OFXSR | CPU_CR4_OSXMMEX);

    cpu_fninit();

    if (!ap) {
        // Detect the MXCSR mask from fxsave context
        isr_fxsave_context_t __aligned(64) fctx;
        cpu_fxsave(&fctx);
        if (fctx.mxcsr_mask)
            default_mxcsr_mask = fctx.mxcsr_mask;
        else
            default_mxcsr_mask = 0xFFBF;
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
}

void cpu_init(int ap)
{
    (void)ap;

    cpu_cr0_change_bits(
                // MP = 0 (Monitor coprocessor task switched)
                // TS = 0 (No task switch pending)
                // EM = 0 (No FPU emulation)
                // CD = 0 (Do not disable cache)
                // NW = 0 (Writeback cache enabled)
                CPU_CR0_MP | CPU_CR0_TS | CPU_CR0_EM | CPU_CR0_CD | CPU_CR0_NW,
                // WP = 1 (Enable write protection in CPL 0)
                // ET = 1 (FPU is not an 80287)
                // NE = 1 (Native FPU error handling, no IRQ)
                // AM = 1 (Allow EFLAGS AC to enable alignment checks in CPL 3)
                CPU_CR0_WP | CPU_CR0_ET | CPU_CR0_NE | CPU_CR0_AM);

    uintptr_t set = 0;
    uintptr_t clr = 0;

    // Supervisor Mode Execution Prevention (SMEP)
    if (cpuid_has_smep())
        set |= CPU_CR4_SMEP;

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

    // Enable {RD|WR}{FS|GS}BASE instructions
    if (cpuid_has_fsgsbase())
        set |= CPU_CR4_FSGSBASE;

    if (cpuid_has_umip()) {
        // Enable user mode instruction prevention
        set |= CPU_CR4_UMIP;
    }

    set |= CPU_CR4_OFXSR | CPU_CR4_OSXMMEX;
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
    cpu_msr_set(CPU_MSR_FMASK, CPU_EFLAGS_AC | CPU_EFLAGS_DF |
                CPU_EFLAGS_TF | CPU_EFLAGS_IF | CPU_EFLAGS_RF |
                CPU_EFLAGS_VM);
    cpu_msr_set(CPU_MSR_LSTAR, uint64_t(syscall_entry));
    cpu_msr_set(CPU_MSR_STAR, (uint64_t(GDT_SEL_KERNEL_CODE64) << 32) |
               (uint64_t(GDT_SEL_USER_CODE32 | 3) << 48));

    // SYSCALL and SYSRET are hardwired to assume these things about the GDT:
    static_assert(GDT_SEL_USER_DATA == GDT_SEL_USER_CODE32 + 8,
                  "GDT inconsistent with SYSCALL/SYSRET behaviour");
    static_assert(GDT_SEL_USER_CODE64 == GDT_SEL_USER_DATA + 8,
                  "GDT inconsistent with SYSCALL/SYSRET behaviour");
    static_assert(GDT_SEL_KERNEL_DATA == GDT_SEL_KERNEL_CODE64 + 8,
                  "GDT inconsistent with SYSCALL/SYSRET behaviour");

    // Load null LDT
    cpu_ldt_set(0);
}

void cpu_init_stage2(int ap)
{
    if (!ap) {
        mmu_init();
        thread_init(ap);
    }
}

void cpu_hw_init(int ap)
{
    printk("Initializing PIT\n");

    // May need PIT nsleep early for APIC calibration
    pit8253_init();

    printk("Initializing APIC\n");

    apic_init(ap);


    // Initialize APIC, but fallback to 8259 if no MP tables
    if (!apic_enable()) {
        printk("Enabling 8259 PIC\n");
        pic8259_enable();
    } else if (acpi_have8259pic()) {
        printk("Disabling 8259 PIC\n");
        pic8259_disable();
    } else {
        panic("No IOAPICs, no MPS, and no 8259! Can't use IRQs! Halting.");
    }

    printk("Initializing RTC\n");

    cmos_init();

    printk("Enabling 8254 PIT\n");

    pit8254_enable();

    printk("Starting SMP\n");

    apic_start_smp();

    //printk("Enabling IRQs\n");
}

static void cpu_init_smp_apic(void *arg)
{
    printdbg("AP in cpu_init_smp_apic\n");
    (void)arg;
    apic_init(1);

    thread_init(1);
}

REGISTER_CALLOUT(cpu_init_smp_apic, nullptr, callout_type_t::smp_start, "200");

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
