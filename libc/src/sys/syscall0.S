.section .text, "ax", @progbits

//scp_t syscall0(scn_t num);
.balign 16
.type syscall0,@function
.global syscall0
syscall0:
	.cfi_startproc

#ifdef __x86_64__
	mov %edi,%eax

#else
	movl 4+8*0(%esp),%eax
#endif

	syscall
	ret

	.cfi_endproc
