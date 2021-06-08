.section .text, "ax", @progbits

//scp_t syscall3(scp_t p0, scp_t p1, scp_t p2, scn_t num);
.balign 16
.type syscall3,@function
.global syscall3
syscall3:
	.cfi_startproc

#if defined(__x86_64__)

	mov %ecx,%eax
	syscall
	ret

#elif defined(__i386__)

	mov 4+8*3(%esp),%eax

	movq 4+8*2(%esp),%xmm2
	movq 4+8*1(%esp),%xmm1
	movq 4+8*0(%esp),%xmm0
	syscall
	ret

#elif defined(__aarch64__)

	mov w8,w3
	svc #0
	ret

#else

#error Unimplemented CPU

#endif


	.cfi_endproc
