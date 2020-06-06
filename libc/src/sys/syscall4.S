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
	// Load 32 bits at a time to always allow store-to-load forwarding

	mov 4+8*4(%esp),%eax

	movd 4+8*3(%esp),%xmm3
	movd 4+8*3+4(%esp),%xmm6
	punpckldq %xmm6,%xmm3

	movd 4+8*2(%esp),%xmm2
	movd 4+8*2+4(%esp),%xmm6
	punpckldq %xmm6,%xmm2

	movd 4+8*1(%esp),%xmm1
	movd 4+8*1+4(%esp),%xmm6
	punpckldq %xmm6,%xmm1

	movd 4+8*0(%esp),%xmm0
	movd 4+8*0+4(%esp),%xmm6
	punpckldq %xmm6,%xmm0
#endif
	syscall
	ret

	.cfi_endproc
