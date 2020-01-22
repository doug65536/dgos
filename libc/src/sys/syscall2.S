.code64
.section .text, "ax", @progbits

//extern long syscall2(long p0, long p1, long num);
.balign 16
.type syscall2,@function
.global syscall2
syscall2:
	.cfi_startproc

	mov %edx,%eax
	syscall
	ret

	.cfi_endproc
