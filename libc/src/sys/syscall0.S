.section .text, "ax", @progbits

//scp_t syscall0(scn_t num);
.balign 16
.type syscall0,@function
.global syscall0
syscall0:
	.cfi_startproc

#if defined(__x86_64__)

	mov %edi,%eax
	syscall
	ret

#elif defined(__i386__)

	movl 4+8*0(%esp),%eax
	syscall
	ret

#elif defined(__aarch64__)

	mov w8,w0
	svc #0
	ret

#else

#error Unimplemented CPU

#endif


	.cfi_endproc
