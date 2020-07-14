.section .text, "ax", @progbits

//scp_t syscall4(scp_t p0, scp_t p1, scp_t p2, scp_t p3, scn_t num);
.balign 16
.type syscall4,@function
.global syscall4
syscall4:
	.cfi_startproc

#ifdef __x86_64__

	mov %r8d,%eax
	mov %rcx,%r10

#else

	mov 4+8*4(%esp),%eax

	movq 4+8*3(%esp),%xmm3
	movq 4+8*2(%esp),%xmm2
	movq 4+8*1(%esp),%xmm1
	movq 4+8*0(%esp),%xmm0

#endif

	syscall
	ret

	.cfi_endproc
