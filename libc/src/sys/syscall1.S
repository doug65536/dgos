.section .text, "ax", @progbits

//scp_t syscall1(scp_t p0, scn_t num);
.balign 16
.type syscall1,@function
.global syscall1
syscall1:
	.cfi_startproc

#if defined(__x86_64__)

	mov %esi,%eax
	syscall
	ret

#elif defined(__i386__)

	movl 4+8*1(%esp),%eax

	movq 4+8*0(%esp),%xmm0
	syscall
	ret

#elif defined(__aarch64__)

	mov w8,w1
	svc #0
	ret

#else

#error Unimplemented CPU

#endif

	.cfi_endproc
