.code64
.section .text

.global _start
_start:
	xor %eax,%eax
	rdtscp
	shl $32,%rdx
	or %rdx,%rax
	mov %rax,%r13
	mov $140,%ebx
	mov $1000000,%r12d
.align 16
0:
	mov %ebx,%eax
	syscall

	mov %ebx,%eax
	syscall

	mov %ebx,%eax
	syscall

	mov %ebx,%eax
	syscall

	mov %ebx,%eax
	syscall

	mov %ebx,%eax
	syscall

	mov %ebx,%eax
	syscall

	mov %ebx,%eax
	syscall

	dec %r12d
	jnz 0b

	xor %eax,%eax
	rdtscp
	shl $32,%rdx
	or %rdx,%rax
	sub %r13,%rax
	mov %rax,%rdi
	mov $60,%eax
	syscall
	jmp _start

test_nanosleep:
	push $0
	push $0
0:
	mov $35,%eax
	mov %rsp,%rdi
	xor %rsi,%rsi
	syscall
	jmp 0b

