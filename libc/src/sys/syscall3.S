.code64
.section .text, "ax"

//extern long syscall3(long p0, long p1, long p2, long num);
.align 32
.global syscall3
syscall3:
	mov %ecx,%eax
	syscall
	ret
