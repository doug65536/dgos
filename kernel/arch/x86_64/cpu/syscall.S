.section .text, "ax"

#include "asm_constants.h"

// This code may clobber rdi, rsi, rdx, rcx, r8-r11, rflags
// This code may clobber the entire FPU/SSE/AVX state
// This code must preserve rbx, rbp, r12-r15
// Note that these are the same rules as function calls

.global syscall_entry
.hidden syscall_entry
syscall_entry:
    .cfi_startproc
    .cfi_register rip,rcx
    .cfi_return_column rcx
    .cfi_def_cfa rsp,0

    // syscall rax
    // params rdi, rsi, rdx, r10, r8, r9
    // return rax
    // CPU puts rip in rcx
    // CPU puts rflags in r11 (which is clobbered)
    // on return, rflags is initialized to CPU_EFLAGS_IF | 2
    // CPU is configured to clear EFLAGS IF, DF, TF, AC on entry

    // Range check syscall number
    cmpq $ SYSCALL_COUNT,%rax
    jae 0f

    // Read function pointer from vector table
    lea syscall_handlers(%rip),%r11
    movq (%r11,%rax,8),%rax

    // Switch to CPU-local data gs
    swapgs

    // Get pointer to current thread from CPU data
    movq %gs:CPU_INFO_CURTHREAD_OFS,%r11

    // Get pointer to syscall stack from thread data
    movq THREAD_SYSCALL_STACK_OFS(%r11),%r11

    // Switch to this thread's syscall stack
    xchgq %rsp,%r11

    .cfi_def_cfa_register r11

    // IRQs are okay at this point
    sti

    // Push user stack pointer to syscall stack
    pushq %r11

    // Push return address to syscall stack
    pushq %rcx

    .cfi_def_cfa rsp,8

    // Move 4th parameter to proper place
    movq %r10,%rcx

    // Call handler
    callq *%rax

    // IRQs are not safe when
    // - we have user gs and still in kernel mode
    // - stack is switched to user stack in kernel mode
    cli

    // Switch to user gs
    swapgs

    // Restore return address
    popq %rcx

    // Set return rflags
    movl $SYSCALL_RFLAGS,%r11d

    // Restore caller's stack
    popq %rsp

    // Don't leak information to user mode
    xorl %edx,%edx
    movl %edx,%esi
    movl %edx,%edi
    movl %edx,%r8d
    movl %edx,%r9d
    movl %edx,%r10d

    sysretq

    // syscall number out of range
0:  movl $SYSCALL_ENOSYS,%eax
    sysretq

    .cfi_endproc
