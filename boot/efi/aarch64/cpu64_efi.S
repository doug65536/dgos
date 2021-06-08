#include "../kernel/arch/aarch64/cfi_helpers.h"


.section .smp.text, "ax", @progbits

// extern "C" _noreturn void arch_run_kernel(
//      uint64_t entry, void *params, uint64_t cr3, bool nx_available)
// x0 = entry
// x1 = params
// x2 = cr3
// w3 = nx_available
.global arch_run_kernel
arch_run_kernel:
    .cfi_startproc

    msr daifset,#0xF

    // Save parameters in call preserved registers,
    // and no point preserving callee saved registers, we are never going back
    mov x20,x0
    mov x21,x1
    mov x22,x2

    // Make sure paging is disabled


    // Switch to kernel page tables
    msr ttbr1_el1,x2
    tlbi vmalle1is

    // Caller and caller's stack (probably) just vanished, let's tell debugger
    no_caller_cfi

    b x0

    .cfi_endproc

.global arch_run_kernel_end
arch_run_kernel_end:

