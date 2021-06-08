#include "x86/cpu_constants.h"
#include "x86/gdt_sel.h"

.code64
.section .smp.text, "ax", @progbits

// extern "C" _noreturn void arch_run_kernel(
//      uint64_t entry, void *params, uint64_t cr3, bool nx_available)
// rdi = entry
// rsi = params
// rdx = cr3
// cl = nx_available
.global arch_run_kernel
arch_run_kernel:
    .cfi_startproc

    cli
    cld

    // Save parameters in call preserved registers,
    // and no point preserving callee saved registers, we are never going back
    mov %rdi,%r15
    .cfi_undefined r15
    mov %rsi,%r14
    .cfi_undefined r14
    mov %rdx,%r13
    .cfi_undefined r13

    // Skip setting EFER.NX if CPU does not support it
    test %cl,%cl
    jz 0f

    // Set EFER.NX
    mov $ CPU_MSR_EFER,%ecx
    rdmsr
    or $ CPU_MSR_EFER_NX,%eax
    wrmsr

    // Switch to kernel page tables
0:  mov %r13,%cr3

    // Caller and caller's stack (probably) just vanished, let's tell debugger
    .cfi_def_cfa rsp,0
    .cfi_undefined rip
    .cfi_undefined rsp

    // Flush global pages
    mov %cr4,%rax
    and $ ~CPU_CR4_PGE,%rax
    mov %rax,%cr4
    or $ CPU_CR4_PGE,%rax
    mov %rax,%cr4

    mov %rdi,%rax
    mov %r14d,%ecx
    jmp *%r15

    .cfi_endproc

.global arch_run_kernel_end
arch_run_kernel_end:

