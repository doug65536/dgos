
.code64
.section .text.code64, "ax", @progbits

// void code64_run_kernel(void *p);

.global code64_run_kernel
code64_run_kernel:
	mov $ 0x4B,%eax
	movw %ax,%ds
	movw %ax,%es
	movw %ax,%fs
	movw %ax,%gs
	xor %eax,%eax
	movw %ax,%ss
	nop
	mov (%rdi),%rax
	mov 8(%rdi),%ecx
	call *%rax
	ret
