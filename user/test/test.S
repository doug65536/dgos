.code64
.section .text

.global _start
_start:
	push $0
	push $0
0:
	mov $35,%eax
	mov %rsp,%rdi
	xor %rsi,%rsi
	syscall
	jmp 0b

