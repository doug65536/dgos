.code64
.section .text, "ax", @progbits

//extern long syscall6(long p0, long p1, long p2, long p3,
//                     long p4, long p5, int num);
.balign 16
.global syscall6
syscall6:
	.cfi_startproc

	mov 8(%rsp),%eax
	mov %rcx,%r10
	syscall
	ret

	.cfi_endproc
