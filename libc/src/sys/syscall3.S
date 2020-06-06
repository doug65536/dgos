.section .text, "ax", @progbits

//scp_t syscall3(scp_t p0, scp_t p1, scp_t p2, scn_t num);
.balign 16
.type syscall3,@function
.global syscall3
syscall3:
	.cfi_startproc

#ifdef __x86_64__
	mov %ecx,%eax

#else
	// Load 32 bits at a time to always allow store-to-load forwarding

	mov 4+8*3(%esp),%eax

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
