#include "cpu_constants.h"
#include "gdt_sel.h"

.code64
.section .smp.text

// extern "C" __noreturn void code64_run_kernel(
//      uint64_t entry, void *params, uint64_t cr3, bool nx_available)
// rdi = entry
// rsi = params
// rdx = cr3
// cl = nx_available
.global code64_run_kernel
code64_run_kernel:
    cli
    cld
    
    // Save parameters in call preserved registers
    mov %rdi,%r15
    mov %rsi,%r14
    mov %rdx,%r13
    
    // See if CPU supports no-execute
    test %cl,%cl
    jz 0f
    
    // Set EFER.NX
    mov $ CPU_MSR_EFER,%ecx
    rdmsr
    or $ CPU_MSR_EFER_NX,%eax
    wrmsr

0:

    // Switch to kernel page tables
    mov %r13,%cr3
    
    // Flush global pages
    mov %cr4,%rax
    and $ ~CPU_CR4_PGE,%rax
    mov %rax,%cr4
    or $ CPU_CR4_PGE,%rax
    mov %rax,%cr4
    
    mov %rdi,%rax
    mov %r14d,%ecx
    jmp *%r15

.global code64_run_kernel_end
code64_run_kernel_end:

