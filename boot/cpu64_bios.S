
.code64
.section .text.code64, "ax", @progbits

// void code64_run_kernel(void *p);
// void code64_reloc_kernel(void *p);
// void code64_copy_kernel(void *p);

.global code64_run_kernel
code64_run_kernel:
	mov $ 0x4B,%eax
	movw %ax,%ds
	movw %ax,%es
	movw %ax,%fs
	movw %ax,%gs
	mov (%rdi),%rax
	mov 8(%rdi),%ecx
	call *%rax
	ret

.global code64_reloc_kernel
code64_reloc_kernel:
	// Distance
	movq (%rdi),%r8

	// Relocations
	movl 8(%rdi),%r9d

	// Relocation count
	movl 12(%rdi),%r10d

	// Read offset and addend
0:	movq (%r9),%r11
	movq 16(%r9),%r12

	// Add adjustment factor to addend
	addq %r8,%r12

	// Write relocated value to relocated offset
	movq %r12,(%r8,%r11)

	addq $ 24,%r9
	decl %r10d
	jnz 0b

	ret

.global code64_copy_kernel
code64_copy_kernel:
	push %rsi
	push %rdi
	mov 8(%rdi),%esi
	mov 12(%rdi),%ecx
	mov (%rdi),%rdi
	rep
	movsb
	pop %rdi
	pop %rsi
	ret
