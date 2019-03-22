.code64

#include <sys/cpu_bug.h>

.section .text, "ax"

.global _start
_start:
	.cfi_startproc

	// Load up call preserved registers before modifying memory at all

    // r12 = argc
    movq (%rsp),%r12
	.cfi_undefined r12

    // r13 = argv
    movq 8(%rsp),%r13
	.cfi_undefined r13

	// r14 = envp = argv + (argc + 1)
    lea 8(%r13,%r12,8),%r14
	.cfi_undefined r14

	// r15 = auxv
    movq %r14,%r15
	.cfi_undefined r15

.Lcheck_envp:
    cmpq $0,(%r15)
    leaq 8(%r15),%r15
    jnz .Lcheck_envp

    // Resolve .rela.iplt
    leaq __rela_iplt_start(%rip),%rbx
	.cfi_undefined rbx

.Lrela_iplt_next:
    leaq __rela_iplt_end(%rip),%rax
	.cfi_undefined rax
	cmpq %rax,%rbx
    jz .Lrela_iplt_done

    // Only apply STT_GNU_IFUNC (0x25)
    cmpl $ 0x25,8(%rbx)
    jnz .Lrela_iplt_skip

    // Call the resolver
	movq 16(%rbx),%rax
	indirect_call rax

    // Read PLT vector pointer from relocation record
    movq (%rbx),%rdx
	.cfi_undefined rdx
	// Update the PLT with the resolver return value
    movq %rax,(%rdx)
.Lrela_iplt_skip:
    addq $ 24,%rbx
.Lrela_iplt_done:

	lea __init_array_start(%rip),%rbx
.Lnext_init:
	leaq __init_array_end(%rip),%rax
	cmpq %rax,%rbx
	je .Ldone_init
	movq (%rbx),%rax
	testq %rax,%rax
	jz .Ldone_init
	call *%rax
	addq $8,%rbx
	jmp .Lnext_init
.Ldone_init:

	mov %rsp,%rbx
	and $-16,%rsp
    call main
	mov %rax,%r12
	mov %rbx,%rsp
    mov %rax,%rdi
.Lexit_returned:
	mov %r12,%rax
    call exit
	jmp .Lexit_returned

	.cfi_endproc

//.section .init_st, "ax"
//    pushq %rbp
//    movq %rsp,%rbp
//
//.section .init_en, "ax"
//    movq %rbp,%rsp
//    popq %rbp
//    ret

