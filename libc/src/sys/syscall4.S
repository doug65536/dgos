.code64
.section .text, "ax"

//extern long syscall4(long p0, long p1, long p2, long p3,
//                     long num);
.balign 16
.global syscall4
syscall4:
	.cfi_startproc

	mov %r8d,%eax
	mov %rcx,%r10
	syscall
	ret

	.cfi_endproc
