.code64
.section .text, "ax", @progbits

//extern long syscall0(long num);
.balign 16
.global syscall0
syscall0:
	.cfi_startproc

	mov %edi,%eax
	syscall
	ret

	.cfi_endproc
