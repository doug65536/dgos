.code64
.section .text, "ax"

//extern long syscall3(long p0, long p1, long p2, long num);
.balign 16
.global syscall3
syscall3:
	.cfi_startproc

	mov %ecx,%eax
	syscall
	ret

	.cfi_endproc
