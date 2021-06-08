.section .text, "ax", @progbits

//scp_t syscall2(scp_t p0, scp_t p1, scn_t num);
.balign 16
.type syscall2,@function
.global syscall2
syscall2:
	.cfi_startproc

#if defined(__x86_64__)

	mov %edx,%eax
	syscall
	ret

#elif defined(__i386__)

	mov 4+8*2(%esp),%eax

	movq 4+8*1(%esp),%xmm1
	movq 4+8*0(%esp),%xmm0
	syscall
	ret

#elif defined(__aarch64__)

	mov w8,w2
	svc #0
	ret

#else

#error Unimplemented CPU

#endif

	.cfi_endproc
