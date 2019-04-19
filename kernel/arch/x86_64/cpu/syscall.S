.section .text, "ax"

#include "asm_constants.h"
#include "cfi_helpers.h"
#include "cpu_bug.h"

// This code may clobber everything. Every single register except
// esp is used to pass parameters
// eax syscall number
// ebx, ecx, edx, esi, edi, ebp 6 parameters
sysenter_entry:
    .cfi_startproc
    cmpl $ SYSCALL_COUNT,%eax
    jae 0f

    // Swizzle 6 x86 parameters into x86_64 parameters
    // edi -> rdi
    // esi -> rsi
    // edx -> rdx
    // ecx -> rcx
    // ebx -> r8
    // ebp -> r9

    movslq %edi,%rdi
    movslq %esi,%rsi
    movslq %edx,%rdx
    movslq %ecx,%rcx
    movslq %ebx,%r8
    movslq %ebp,%r9



    sysexit

    .cfi_endproc

// This code may clobber rdi, rsi, rdx, rcx, r8-r11, rflags
// This code may clobber the entire FPU/SSE/AVX state
// This code must preserve rbx, rbp, r12-r15
// Note that these are the same rules as x86_64-elf function calls

.global syscall_entry
.hidden syscall_entry
syscall_entry:
    .cfi_startproc
    .cfi_signal_frame
    .cfi_register rip,rcx
    .cfi_register rsp,rsp
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

    .cfi_def_cfa r11,0
    .cfi_register rsp,r11

    // IRQs are okay at this point
    sti

    // Push user stack pointer to syscall stack
    pushq %r11
    .cfi_def_cfa rsp,0
    .cfi_offset rsp,-1*8

    // Push return address to syscall stack
    push_cfi %rcx
    .cfi_offset rip,-2*8

    // Prevent user mode from controlling rbx,rbp,r12,r13,r14,r15

    push_cfi %rbp
    .cfi_offset rbp,-3*8
    xorl %ebp,%ebp

    push_cfi %rbx
    .cfi_offset rbx,-4*8
    movl %ebp,%ebx

    push_cfi %r12
    .cfi_offset r12,-5*8
    movl %ebp,%r12d

    push_cfi %r13
    .cfi_offset r13,-6*8
    movl %ebp,%r13d

    push_cfi %r14
    .cfi_offset r14,-7*8
    movl %ebp,%r14d

    push_cfi %r15
    .cfi_offset r15,-8*8
    movl %ebp,%r15d

    // Move 4th parameter to proper place
    movq %r10,%rcx

    // Call handler
    indirect_call rax

    pop_cfi %r15
    .cfi_same_value r15

    pop_cfi %r14
    .cfi_same_value r14

    pop_cfi %r13
    .cfi_same_value r13

    pop_cfi %r12
    .cfi_same_value r12

    pop_cfi %rbx
    .cfi_same_value rbx

    pop_cfi %rbp
    .cfi_same_value rbp

    // IRQs are not safe when
    // - we have user gs and still in kernel mode
    // - stack is switched to user stack in kernel mode
    cli

    // Switch to user gs
    swapgs

    // Restore return address
    pop_cfi %rcx
    .cfi_register rip,rcx

    // Set return rflags
    movl $SYSCALL_RFLAGS,%r11d

    // Restore caller's stack
    pop_cfi %rsp
    .cfi_def_cfa rsp,0

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
