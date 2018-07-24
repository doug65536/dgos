.code64
.section .text

//extern long syscall2(long p0, long p1, long num);
.global syscall2
syscall2:
	mov %edx,%eax
	syscall
	ret
