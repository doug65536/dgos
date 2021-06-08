.section .text, "ax", @progbits

//scp_t syscall6(scp_t p0, scp_t p1, scp_t p2, scp_t p3,
//               scp_t p4, scp_t p5, scn_t nr);
.balign 16
.type syscall6,@function
.global syscall6
syscall6:
	.cfi_startproc

#if defined(__x86_64__)

	mov 8(%rsp),%eax
	mov %rcx,%r10
	syscall
	ret

#elif defined(__i386__)

	mov 4+8*6(%esp),%eax

	movq 4+8*5(%esp),%xmm5
	movq 4+8*4(%esp),%xmm4
	movq 4+8*3(%esp),%xmm3
	movq 4+8*2(%esp),%xmm2
	movq 4+8*1(%esp),%xmm1
	movq 4+8*0(%esp),%xmm0
	syscall
	ret

#elif defined(__aarch64__)

	mov w8,w6
	svc #0
	ret

#else

#error Unimplemented CPU

#endif


	.cfi_endproc
