
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

void cpu_init(int ap)
{
    (void)ap;

    // Enable write protection
    cpu_cr0_change_bits(0, CR0_WP);

    uintptr_t cr4_set = 0;
    uintptr_t cr4_clr = 0;

    // Supervisor Mode Execution Prevention (SMEP)
    if (cpuid_has_smep())
        cr4_set |= CR4_SMEP;

    // Enable global pages feature if available
    if (cpuid_has_pge())
        cr4_set |= CR4_PGE;

    // Enable debugging extensions feature if available
    if (cpuid_has_de())
        cr4_set |= CR4_DE;

    // Disable paging context identifiers feature if available
    if (cpuid_has_pcid())
        cr4_clr |= CR4_PCIDE;

    cr4_set |= CR4_OFXSR | CR4_OSXMMEX;
    cr4_clr |= CR4_TSD;

    cpu_cr4_change_bits(cr4_clr, cr4_set);

    // Enable no-execute if feature available
    if (cpuid_has_nx())
        msr_adj_bit(MSR_EFER, 11, 1);
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
    idt_init(ap);
    gdt_init();
    mmu_init(ap);
    if (!ap) {
        thread_init(ap);
        //intr_hook(INTR_EX_DEBUG, cpu_debug_exception_handler);
    }
}

void cpu_hw_init(int ap)
{
    // May need PIT nsleep early for APIC calibration
    pit8253_init();

    apic_init(ap);

    // Initialize APIC, but fallback to 8259 if no MP tables
    if (!apic_enable())
        pic8259_enable();
    else if (acpi_have8259pic())
        pic8259_disable();
    else
        panic("No IOAPICs, no MPS, and no 8259! Can't use IRQs! Halting.");

    cmos_init();

    pit8254_enable();

    cpu_irq_enable();

    apic_start_smp();
}

static void cpu_init_smp_apic(void *arg)
{
    printdbg("AP in cpu_init_smp_apic\n");
    (void)arg;
    apic_init(1);

    thread_init(1);
}

REGISTER_CALLOUT(cpu_init_smp_apic, 0, callout_type_t::smp_start, "200");
