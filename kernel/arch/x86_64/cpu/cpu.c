
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

void cpu_init(int ap)
{
    // Enable write protection
    cpu_cr0_change_bits(0, CR0_WP);

    uint64_t cr4 = 0;

    // Supervisor Mode Execution Prevention (SMEP)
    if (cpuid_ebx_bit(7, 7, 0))
        cr4 |= CR4_SMEP;

    // Enable global pages feature if available
    if (cpuid_edx_bit(13, 1, 0))
        cr4 |= CR4_PGE;

    // Enable debugging extensions feature if available
    if (cpuid_edx_bit(2, 1, 0))
        cr4 |= CR4_DE;

    // Enable paging context identifiers feature if available
    if (cpuid_ecx_bit(17, 1, 0))
        cr4 |= CR4_PCIDE;

    // Enable XSAVE if feature available
    //if (cpuid_ecx_bit(26, 1, 0))
    //    cr4 |= CR4_OSXSAVE;

    cpu_cr4_change_bits(CR4_TSD, cr4 | CR4_OFXSR | CR4_OSXMMEX);

    // Enable no-execute if feature available
    if (cpuid_edx_bit(20, 0x80000001, 0))
        msr_adj_bit(MSR_EFER, 11, 1);

    gdt_init();
    idt_init(ap);
    mmu_init(ap);
    if (!ap)
        thread_init(ap);
}

void cpu_hw_init(int ap)
{
    //if (ap) {
    //    gdt_load_tr(thread_cpus_started());
    //}

    apic_init(ap);
    cmos_init();

    pic8259_enable();
    pit8254_enable();

    apic_start_smp();
}

static void cpu_init_smp_apic(void *arg)
{
    (void)arg;
    apic_init(1);
    thread_init(1);
}

REGISTER_CALLOUT(cpu_init_smp_apic, 0, 'S', "200");
