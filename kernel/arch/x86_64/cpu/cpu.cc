
#include "cpu.h"
#include "mmu.h"
#include "gdt.h"
#include "idt.h"
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
#include "interrupts.h"
#include "syscall.h"
#include "string.h"

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

    // Enable global pages feature if available
    if (cpuid_has_pge())
        set |= CPU_CR4_PGE;

    // Enable debugging extensions feature if available
    if (cpuid_has_de())
        set |= CPU_CR4_DE;

    // Disable paging context identifiers feature if available
    if (cpuid_has_pcid())
        clr |= CPU_CR4_PCIDE;

    // Enable {RD|WR}{FS|GS}BASE instructions
    if (cpuid_has_fsgsbase())
        set |= CPU_CR4_FSGSBASE;

    set |= CPU_CR4_OFXSR | CPU_CR4_OSXMMEX;
    clr |= CPU_CR4_TSD;

    cpu_cr4_change_bits(clr, set);

    //
    // Adjust IA32_MISC_ENABLES

    // crashes on non-intel
//    set = CPU_MSR_MISC_ENABLE_FAST_STR;
//    clr = CPU_MSR_MISC_ENABLE_LIMIT_CPUID;
//
//    if (cpuid_has_mwait())
//        set = CPU_MSR_MISC_ENABLE_MONITOR_FSM;
//
//    if (cpuid_has_nx())
//        clr = CPU_MSR_MISC_ENABLE_XD_DISABLE;
//
//    cpu_msr_change_bits(CPU_MSR_MISC_ENABLE, clr, set);

    // Enable no-execute if feature available
    if (cpuid_has_nx())
        cpu_msr_change_bits(CPU_MSR_EFER, 0, CPU_MSR_EFER_NX);

    // Configure syscall
    cpu_msr_set(CPU_MSR_FMASK, CPU_EFLAGS_AC | CPU_EFLAGS_DF |
                CPU_EFLAGS_TF | CPU_EFLAGS_IF);
    cpu_msr_set(CPU_MSR_LSTAR, (uint64_t)syscall_entry);
    cpu_msr_set_hi(CPU_MSR_STAR, GDT_SEL_KERNEL_CODE64 |
               (GDT_SEL_USER_CODE32 << 16));
}

//static isr_context_t *cpu_debug_exception_handler(int intr, isr_context_t *ctx)
//{
//    cpu_debug_break();
//    assert(intr == INTR_EX_DEBUG);
//    printdbg("Unexpected debug exception, continuing\n");
//    return ctx;
//}

void cpu_init_stage2(int ap)
{
    mmu_init(ap);
    if (!ap) {
        thread_init(ap);
        //intr_hook(INTR_EX_DEBUG, cpu_debug_exception_handler);
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

    printk("Enabling IRQs\n");

    cpu_irq_enable();
}

static void cpu_init_smp_apic(void *arg)
{
    printdbg("AP in cpu_init_smp_apic\n");
    (void)arg;
    apic_init(1);

    thread_init(1);
}

REGISTER_CALLOUT(cpu_init_smp_apic, 0, callout_type_t::smp_start, "200");

void cpu_patch_insn(void *addr, uint64_t value, size_t size)
{
    // Disable write protect
    cpu_cr0_change_bits(CPU_CR0_WP, 0);
    // Patch
    memcpy(addr, &value, size);
    // Enable write protect
    cpu_cr0_change_bits(0, CPU_CR0_WP);
}
