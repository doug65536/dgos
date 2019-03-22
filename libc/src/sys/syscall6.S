.code64
.section .text, "ax"

//extern long syscall6(long p0, long p1, long p2, long p3,
//                     long p4, long p5, int num);
.global syscall6
syscall6:
	mov 8(%rsp),%eax
	mov %rcx,%r10
	syscall
	ret
