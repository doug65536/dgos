#include "cfi_helpers.h"

.section .text, "ax"

.balign 16
.global __exception_setjmp
__exception_setjmp:
    .cfi_startproc

    // Save call preserved registers
    movq %rbx,2*8(%rdi)
    movq %rbp,3*8(%rdi)
    movq %r12,4*8(%rdi)
    movq %r13,5*8(%rdi)
    movq %r14,6*8(%rdi)
    movq %r15,7*8(%rdi)

    // Save flags
    pushfq
    .cfi_adjust_cfa_offset 8
    popq 8*8(%rdi)
    .cfi_adjust_cfa_offset -8

    // Save return address
    movq (%rsp),%rax
    movq %rax,(%rdi)

    // Save callers stack pointer
    leaq 8(%rsp),%rax
    movq %rax,1*8(%rdi)

    // Return value is zero
    xorl %eax,%eax
    ret

    .cfi_endproc

.balign 16
.global __exception_longjmp
__exception_longjmp:
    .cfi_startproc

    // Restore call preserved registers
    movq 2*8(%rdi),%rbx
    movq 3*8(%rdi),%rbp
    xorl %r11d,%r11d
    xorl %r10d,%r10d
    movq 4*8(%rdi),%r12
    movq 5*8(%rdi),%r13
    xorl %r9d,%r9d
    xorl %r8d,%r8d
    movq 6*8(%rdi),%r14
    movq 7*8(%rdi),%r15
    xorl %edx,%edx
    xorl %ecx,%ecx

    // Restore flags
    push_cfi 8*8(%rdi)
    popfq_cfi

    // Restore stack pointer
    movq 1*8(%rdi),%rsp

    // Return value is second parameter and "return" from setjmp again
    movq %rsi,%rax
    jmpq *(%rdi)

    .cfi_endproc

// void __exception_longjmp_unwind(__exception_jmp_buf_t *__ctx, int value);
.balign 16
.global __exception_longjmp_unwind
__exception_longjmp_unwind:
    .cfi_startproc

    push_cfi %rbx
    .cfi_offset rbx,-16
    push_cfi %r12
    .cfi_offset r12,-24
    adj_rsp -8
    movq %rdi,%rbx
    movq %rsi,%r12

    // exception object (1st parameter)
    mov $1024,%rdi
    call __cxa_allocate_exception
    mov %rax,%rdi

    // Pass jmpbuf as stop (3rd) parameter
    movq %rbx,%rdx
    // stopfn is 2nd parameter
    leaq __exception_stop_fn(%rip),%rsi
    call _Unwind_ForcedUnwind
    ud2

    .cfi_endproc
