.section .text, "ax"

.global __exception_setjmp
.hidden __exception_setjmp
__exception_setjmp:
	# Save call preserved registers
	movq %rbx,2*8(%rdi)
	movq %rbp,3*8(%rdi)
	movq %r12,4*8(%rdi)
	movq %r13,5*8(%rdi)
	movq %r14,6*8(%rdi)
	movq %r15,7*8(%rdi)

	# Save flags
	pushfq
	popq 8*8(%rdi)

	# Save return address
	movq (%rsp),%rax
	movq %rax,(%rdi)

	# Save callers stack pointer
	leaq 8(%rsp),%rax
	movq %rax,1*8(%rdi)

	# Return value is zero
	xorl %eax,%eax
	ret

.global __exception_longjmp
.hidden __exception_longjmp
__exception_longjmp:
	# Restore call preserved registers
	movq 2*8(%rdi),%rbx
	movq 3*8(%rdi),%rbp
	movq 4*8(%rdi),%r12
	movq 5*8(%rdi),%r13
	movq 6*8(%rdi),%r14
	movq 7*8(%rdi),%r15

	# Restore flags
	pushq 8*8(%rdi)
	popfq

	# Restore stack pointer
	movq 1*8(%rdi),%rsp

	# Return value is second parameter and "return" from setjmp again
	movq %rsi,%rax
	jmpq *(%rdi)
