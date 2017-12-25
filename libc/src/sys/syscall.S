.code64
.section .text

//extern long syscall6(long p0, long p1, long p2, long p3,
//                     long p4, long p5, int num);
.global syscall6
syscall6:
	mov 8(%rsp),%eax
	mov %rcx,%r10
	syscall
	ret

//extern long syscall5(long p0, long p1, long p2, long p3,
//                     long p4, long num);
.global syscall5
syscall5:
	mov %r9d,%eax
	mov %rcx,%r10
	syscall
	ret

//extern long syscall4(long p0, long p1, long p2, long p3,
//                     long num);
.global syscall4
syscall4:
	mov %r8d,%eax
	mov %rcx,%r10
	syscall
	ret

//extern long syscall3(long p0, long p1, long p2, long num);
.align 32
.global syscall3
syscall3:
	mov %ecx,%eax
	syscall
	ret

//extern long syscall2(long p0, long p1, long num);
.global syscall2
syscall2:
	mov %edx,%eax
	syscall
	ret

//extern long syscall1(long p0, long num);
.global syscall1
syscall1:
	mov %esi,%eax
	syscall
	ret

//extern long syscall0(long num);
.align 16
.global syscall0
syscall0:
	mov %edi,%eax
	syscall
	ret

