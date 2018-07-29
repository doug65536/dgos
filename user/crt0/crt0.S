.code64

.section .text

.global _start
_start:
    // r12 = argc
    movq (%rsp),%r12
    // r13 = argv
    movq 8(%rsp),%r13
    // r14 = envp = argv + (argc + 1)
    lea 8(%r13,%r12,8),%r14
    // r15 = auxv
    movq %r14,%r15
.Lcheck_envp:
    cmpq $0,(%r15)
    leaq 8(%r15),%r15
    jnz .Lcheck_envp

    // Resolve .rela.iplt
    leaq __rela_iplt_start(%rip),%rbx
.Lrela_iplt_next:
    leaq __rela_iplt_end(%rip),%rax
    cmpq %rax,%rbx
    jz .Lrela_iplt_done

    // Only apply STT_GNU_IFUNC (0x25)
    cmpl $ 0x25,8(%rbx)
    jnz .Lrela_iplt_skip

    // Call the resolver
    call * 16(%rbx)

    // Read PLT vector pointer from relocation record
    movq (%rbx),%rdx
    // Update the PLT with the resolver return value
    movq %rax,(%rdx)
.Lrela_iplt_skip:
    addq $ 24,%rbx
.Lrela_iplt_done:

    lea __init_array_start(%rip),%rbx
.Lnext_ctor:
    leaq __init_array_end(%rip),%rax
    cmpq %rax,%rbx
    je .Ldone_ctor
    movq (%rbx),%rax
    testq %rax,%rax
    jz .Ldone_ctor
    call *%rax
    addq $8,%rbx
    jmp .Lnext_ctor
.Ldone_ctor:

    call main
    mov %rax,%rdi
    call exit

#.section .init_st
#    pushq %rbp
#    movq %rsp,%rbp
#
#.section .init_en
#    movq %rbp,%rsp
#    popq %rbp
#    ret

