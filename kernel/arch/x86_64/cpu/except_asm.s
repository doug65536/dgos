.section .text, "ax"

.global __exception_setjmp
.hidden __exception_setjmp
__exception_setjmp:
	# Save return address
	mov (%rsp),%rax
	mov %rax,(%rdi)

	# Save callers stack pointer
	lea 8(%rsp),%rax
	mov %rax,1*8(%rdi)

	# Save call preserved registers
	mov %rbx,2*8(%rdi)
	mov %rbp,3*8(%rdi)
	mov %r12,4*8(%rdi)
	mov %r13,5*8(%rdi)
	mov %r14,6*8(%rdi)
	mov %r15,7*8(%rdi)
	pushf
	popq 8*8(%rdi)
	xor %eax,%eax
	ret

.global __exception_longjmp
.hidden __exception_longjmp
__exception_longjmp:
	# Restore call preserved registers
	mov 2*8(%rdi),%rbx
	mov 3*8(%rdi),%rbp
	mov 4*8(%rdi),%r12
	mov 5*8(%rdi),%r13
	mov 6*8(%rdi),%r14
	mov 7*8(%rdi),%r15
	pushq 8*8(%rdi)
	popf
	# Return value is second parameter
	mov %rsi,%rax
	# Restore stack pointer and "return" from setjmp again
	mov 1*8(%rdi),%rsp
	jmp *(%rdi)
