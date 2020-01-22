.code64
.section .text, "ax", @progbits

//extern long syscall5(long p0, long p1, long p2, long p3,
//                     long p4, long num);
.balign 16
.type syscall5,@function
.global syscall5
syscall5:
	.cfi_startproc

	mov %r9d,%eax
	mov %rcx,%r10
	syscall
	ret

	.cfi_endproc
