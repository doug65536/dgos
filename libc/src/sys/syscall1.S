.code64
.section .text

//extern long syscall1(long p0, long num);
.global syscall1
syscall1:
	mov %esi,%eax
	syscall
	ret
