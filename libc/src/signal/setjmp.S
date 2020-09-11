#if defined(__x86_64__)

.global setjmp
setjmp:
    .cfi_startproc

    mov (%rsp),%rdx
    .cfi_adjust_cfa_offset 8
    lea 8(%rsp),%rcx
    // rbx, rbp, r12-r15
    mov %rbx,(%rdi)
    mov %rbp,8*1(%rdi)
    mov %r12,8*2(%rdi)
    mov %r13,8*3(%rdi)
    mov %r14,8*4(%rdi)
    mov %r15,8*5(%rdi)
    // rip, rsp
    mov %rdx,8*6(%rdi)
    mov %rcx,8*7(%rdi)
    .cfi_adjust_cfa_offset -8
    xor %eax,%eax
    ret

    .cfi_endproc

.global longjmp
longjmp:
    .cfi_startproc

    mov    (%rdi),%rbx
    .cfi_undefined rbx

    mov 8*1(%rdi),%rbp
    .cfi_undefined rbp

    mov 8*2(%rdi),%r12
    .cfi_undefined r12

    mov 8*3(%rdi),%r13
    .cfi_undefined r13

    mov 8*4(%rdi),%r14
    .cfi_undefined r14

    mov 8*5(%rdi),%r15
    .cfi_undefined r15

    mov 8*6(%rdi),%rdx
    mov 8*7(%rdi),%rcx

    mov %rcx,%rsp
    .cfi_same_value rsp
    .cfi_register rip,rdx

    // longjmp second argument becomes setjmp return value
    mov %rsi,%rax

    // Return from setjmp, again
    jmpq *%rdx

    .cfi_endproc

#elif defined(__i386__)

.global setjmp
setjmp:
    .cfi_startproc

    mov 4(%esp),%eax
    lea 4(%esp),%ecx

    // ebx, esi, edi, ebp
    mov %ebx,(%eax)
    mov %esi,8*1(%eax)
    mov %edi,8*2(%eax)
    mov %ebp,8*3(%eax)

    // eip, esp
    mov %edx,8*4(%eax)
    mov %ecx,8*5(%eax)

    xor %eax,%eax
    ret

    .cfi_endproc

.global longjmp
longjmp:
    .cfi_startproc

    mov 4(%esp),%eax

    mov    (%eax),%ebx
    .cfi_undefined ebx

    mov 4*1(%eax),%esi
    .cfi_undefined esi

    mov 4*2(%eax),%edi
    .cfi_undefined edi

    mov 4*3(%eax),%ebp
    .cfi_undefined ebp

    // eip, esp
    mov 4*6(%eax),%edx
    .cfi_register eip,edx

    mov 4*7(%eax),%ecx
    .cfi_register esp,ecx

    mov %edi,%eax
    mov %ecx,%esp
    .cfi_register esp,esp

    jmp *%edx

    .cfi_endproc

#endif
