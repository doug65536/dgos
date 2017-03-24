
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

void cpu_init(int ap)
{
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

    gdt_init();
    idt_init(ap);
    mmu_init(ap);
    if (!ap)
        thread_init(ap);
}

void cpu_hw_init(int ap)
{
    apic_init(ap);
    cmos_init();

    // Initialize APIC, but fallback to 8259 if no MP tables
    if (!apic_enable())
        pic8259_enable();
    else if (acpi_have8259pic())
        pic8259_disable();
    else
        panic("No IOAPICs, no MPS, and no 8259! Can't use IRQs! Halting.");

    pit8254_enable();

    cpu_irq_enable();

    apic_start_smp();
}

static void cpu_init_smp_apic(void *arg)
{
    (void)arg;
    apic_init(1);
    thread_init(1);
}

REGISTER_CALLOUT(cpu_init_smp_apic, 0, 'S', "200");
