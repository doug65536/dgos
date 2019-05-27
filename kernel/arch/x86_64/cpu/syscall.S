.section .text, "ax"

#include "control_regs_constants.h"
#include "asm_constants.h"
#include "cfi_helpers.h"
#include "cpu_bug.h"
#include "fixup.h"

// i386 syscall ABI design notes
//  - syscall number in eax
//  - edx then ecx are preferred to be used first
//  - then ebx, esi, edi, ebp

// +------+--------+
// | i386 | x86_64 |
// +------+--------+
// |  eax |   eax  | syscall number
// |  edx |  [rcx] | return address \---+--> matches sysexit instruction
// |  ecx |  [rsp] | caller's esp   /--/
// | xmm0 |   rdi  | 1st parameter
// | xmm1 |   rsi  | 2nd parameter
// | xmm2 |   rdx  | 3rd parameter
// | xmm3 |   rcx  | 4th parameter (r10 in syscall interface, rcx in function)
// | xmm4 |   r8   | 5th parameter
// | xmm5 |   r9   | 6th parameter
// +------+--------+

// incoming:
// eax syscall number, ecx return address, edx return stack pointer
// xmm0-xmm5 6 parameters in low 64 bits
.global syscall32_entry
.hidden syscall32_entry
syscall32_entry:
    .cfi_startproc
    .cfi_signal_frame
    .cfi_def_cfa rsp,0
    .cfi_register rip,rcx
    .cfi_register rsp,rsp
    .cfi_undefined r12
    .cfi_undefined r13
    .cfi_undefined r14
    .cfi_undefined r15

    // We can clobber r12, r13, r14, r15 without saving them,
    // because the caller is 32 bit code
    // Take advantage of that by saving caller's esp, eip, edi, esi
    // in r15, r14, r13, r12, avoiding the stack

    // Range check syscall number
    cmp $ SYSCALL_COUNT,%rax
    jae 0f

    // Switch to kernel gs
    swapgs

    // Save return eip in r14d
    mov %ecx,%r14d
    .cfi_register rip,r14

    // Get pointer to current CPU's TSS from CPU-local data
    movq %gs:CPU_INFO_TSS_PTR_OFS,%r15

    movq %xmm5,%r9

    lea syscall_handlers(%rip),%r11

    movq %xmm4,%r8

    // Save esi in r12
    mov %esi,%r12d
    .cfi_register rsi,r12

    movq %xmm3,%rcx

    // Get pointer to syscall stack from thread data
    movq TSS_RSP0_OFS(%r15),%r15

    movq %xmm2,%rdx

    // Read the syscall vector
    movq (%r11,%rax,8),%rax

    movq %xmm1,%rsi

    // Switch to this thread's syscall stack
    // and save caller's esp in r15d
    xchgq %r15,%rsp
    .cfi_def_cfa rsp,0
    .cfi_register rip,rcx
    .cfi_register rsp,r14

    // IRQs are okay at this point
    sti

    // Save edi in r13
    mov %edi,%r13d
    .cfi_register rdi,r13

    movq %xmm0,%rdi

    indirect_call rax

    mov %gs:CPU_INFO_CURTHREAD_OFS,%rdi
    cli
    swapgs

    vzeroall_insn
    emms
    fldz
    fldz
    fldz
    fldz
    ldmxcsr THREAD_SC_MXCSR_OFS(%rdi)
    fldz
    fldz
    fldz
    fldz
    emms
    fldcw THREAD_SC_FCW87_OFS(%rdi)

    // Get return address back into ecx
    mov %r14d,%ecx

    // Get return stack pointer back into edx
    mov %r15d,%edx

    mov %r13d,%edi
    mov %r12d,%esi

    sysretl

    .cfi_endproc

// This code may clobber rdi, rsi, rdx, rcx, r8-r11, rflags
// This code may clobber the entire FPU/SSE/AVX state (except control words)
// This code must preserve rbx, rbp, r12-r15
// Note that these are the same rules as x86_64-elf function calls

.global syscall_entry
.hidden syscall_entry
syscall_entry:
    .cfi_startproc
    .cfi_signal_frame
    .cfi_def_cfa rsp,0
    .cfi_register rip,rcx
    .cfi_register rsp,rsp

    // syscall rax
    // params rdi, rsi, rdx, r10, r8, r9
    // return rax
    // CPU puts rip in rcx
    // CPU puts rflags in r11 (which is clobbered)
    // on return, rflags is initialized to CPU_EFLAGS_IF | 2
    // CPU is configured to clear EFLAGS IF, DF, TF, AC on entry
    // still on the caller's stack though!

    // Range check syscall number
    cmpq $ SYSCALL_COUNT,%rax
    jae 0f

    // Read function pointer from vector table
    lea syscall_handlers(%rip),%r11
    movq (%r11,%rax,8),%rax

    // Switch to kernel gs
    swapgs

    // Get pointer to current thread
    movq %gs:CPU_INFO_CURTHREAD_OFS,%r11

    // Store SSE and x87 control words
    stmxcsr THREAD_SC_MXCSR_OFS(%r11)
    fnstcw THREAD_SC_FCW87_OFS(%r11)

    // Get pointer to current CPU's TSS from CPU-local data
    movq %gs:CPU_INFO_TSS_PTR_OFS,%r11

    // Get pointer to syscall stack from thread data
    movq TSS_RSP0_OFS(%r11),%r11

    // Switch to this thread's syscall stack
    xchgq %rsp,%r11

    .cfi_def_cfa r11,0
    .cfi_register rsp,r11

    // IRQs are okay at this point
    sti

    // Push user stack pointer to syscall stack
    pushq %r11
    .cfi_def_cfa rsp,8
    .cfi_offset rsp,-1*8

    // Push return address to syscall stack
    push_cfi %rcx
    .cfi_offset rip,-2*8

    // call IBPB or patched with nop if not available
    insn_fixup
    call protection_barrier

    // Move 4th parameter to proper place
    movq %r10,%rcx

    // Call handler
    indirect_call rax

    movq %gs:CPU_INFO_CURTHREAD_OFS,%rdi

    // IRQs are not safe when
    // - we have user gs and still in kernel mode
    // - stack is switched to user stack in kernel mode
    cli

    // Switch to user gs
    swapgs

    // Don't leak information to user mode, clear SSE, clear x87
    vzeroall_insn
    emms

    // Restore return address
    pop_cfi %rcx
    .cfi_register rip,rcx

    fldz

    // Set return rflags
    movl $ SYSCALL_RFLAGS,%r11d

    fldz

    insn_fixup
    call protection_barrier

    // Restore caller's stack
    pop_cfi %rsp
    .cfi_def_cfa rsp,0
    .cfi_register rsp,rsp

    xorl %edx,%edx
    fldz
    movl %edx,%esi
    fldz
    movl %edx,%r8d
    fldz
    movl %edx,%r9d
    fldz
    movl %edx,%r10d
    fldz
    ldmxcsr THREAD_SC_MXCSR_OFS(%rdi)
    fldz
    emms
    fldcw THREAD_SC_FCW87_OFS(%rdi)
    mov %edx,%edi

    sysretq

    // syscall number out of range
0:  movl $ SYSCALL_ENOSYS,%eax
    sysretq

    .cfi_endproc
