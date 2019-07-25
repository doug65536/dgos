.code64
.section .text, "ax", @progbits

//extern long syscall1(long p0, long num);
.balign 16
.global syscall1
syscall1:
	.cfi_startproc

	mov %esi,%eax
	syscall
	ret

	.cfi_endproc
