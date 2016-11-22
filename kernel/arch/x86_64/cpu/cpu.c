
#include "cpu.h"
#include "mmu.h"
#include "gdt.h"
#include "idt.h"
#include "control_regs.h"
#include "legacy_pic.h"
#include "legacy_pit.h"
#include "thread_impl.h"
#include "apic.h"
#include "cpuid.h"

void cpu_init(void)
{
    cpu_cr0_change_bits(0,
                        CR0_WP |
                        CR0_NW);

    uint64_t cr4 = 0;

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
    if (cpuid_ecx_bit(26, 1, 0))
        cr4 |= CR4_OSXSAVE;

    cpu_cr4_change_bits(CR4_TSD,
                        cr4 |
                        CR4_OFXSR |
                        CR4_OSXMMEX);

    //init_gdt();
    idt_init();
    thread_init();

    if (0 && cpuid_edx_bit(9, 1, 0)) {
        apic_init();

        // Still need to initialize in case of
        // spurious IRQs
        pic8259_disable();

        // Initialize APIC timer...
    } else {
        pic8259_enable();
        pit8254_enable();
    }
}
