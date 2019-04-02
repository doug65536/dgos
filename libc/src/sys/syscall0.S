.code64
.section .text, "ax"

//extern long syscall0(long num);
.align 16
.global syscall0
syscall0:
	.cfi_startproc

	mov %edi,%eax
	syscall
	ret

	.cfi_endproc
