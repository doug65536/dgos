.code64
.section .text

//extern long syscall0(long num);
.align 16
.global syscall0
syscall0:
	mov %edi,%eax
	syscall
	ret

