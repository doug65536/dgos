#include "cfi_helpers.h"

.section .text, "ax", @progbits

// intptr_t __setjmp(__exception_jmp_buf_t *__ctx);
.balign 16
.global __setjmp
__setjmp:
    .cfi_startproc

    mrs x1,daif
    mov x2,sp
    stp  x1, x2,[x0]
    stp x30,x29,[x0, 1*16]
    stp x28,x27,[x0, 2*16]
    stp x26,x25,[x0, 3*16]
    stp x24,x23,[x0, 4*16]
    stp x22,x21,[x0, 5*16]
    stp x20,x19,[x0, 6*16]

    // Return value is zero
    mov x0,0
    ret

    .cfi_endproc

// void __longjmp(__exception_jmp_buf_t *__ctx, int value);
.balign 16
.global __longjmp
__longjmp:
    .cfi_startproc

    ldp  x2, x3,[x0]
    ldp x30,x29,[x0, 1*16]
    ldp x28,x27,[x0, 2*16]
    ldp x26,x25,[x0, 3*16]
    ldp x24,x23,[x0, 4*16]
    ldp x22,x21,[x0, 5*16]
    ldp x20,x19,[x0, 6*16]

    mov sp,x3
    msr daif,x2
    mov x0,x1
#ifdef PARANOID
    mov x3,0
    mov x1,x3
    mov x2,x3
    mov x4,x3
    mov x5,x3
    mov x6,x3
    mov x7,x3
    mov x8,x3
    mov x9,x3
    mov x10,x3
    mov x11,x3
    mov x12,x3
    mov x13,x3
    mov x14,x3
    mov x15,x3
    mov x16,x3
    mov x17,x3
#endif
    ret

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
    adj_rsp_cfi -8
    movq %rdi,%rbx
    movq %rsi,%r12

    // exception object (1st parameter)
    mov $ 1024,%rdi
    call __cxa_allocate_exception
    mov %rax,%rdi

    // Pass jmpbuf as stop (3rd) parameter
    movq %rbx,%rdx
    // stopfn is 2nd parameter
    leaq __exception_stop_fn(%rip),%rsi
    call _Unwind_ForcedUnwind
    ud2

    .cfi_endproc
