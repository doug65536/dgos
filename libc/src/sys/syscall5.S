.section .text, "ax", @progbits

//scp_t syscall5(scp_t p0, scp_t p1, scp_t p2, scp_t p3, scp_t p4, scn_t num);
.balign 16
.type syscall5,@function
.global syscall5
syscall5:
	.cfi_startproc

#ifdef __x86_64__

	mov %r9d,%eax
	mov %rcx,%r10

#else

	mov 4+8*5(%esp),%eax

	movq 4+8*4(%esp),%xmm4
	movq 4+8*3(%esp),%xmm3
	movq 4+8*2(%esp),%xmm2
	movq 4+8*1(%esp),%xmm1
	movq 4+8*0(%esp),%xmm0

#endif

	syscall
	ret

	.cfi_endproc
