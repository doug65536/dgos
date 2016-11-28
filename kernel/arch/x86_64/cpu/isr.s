.text

# Handler that pushes whole state
.macro isr_exception_entry has_code int_num
.globl isr_entry_\int_num\()
isr_entry_\int_num\():
	.if \has_code == 0
		pushq $0
	.endif
	pushq $\int_num
	jmp exception_common
.endm

.macro isr_entry has_code int_num
.globl isr_entry_\int_num\()
isr_entry_\int_num\():
	.if \has_code == 0
		pushq $0
	.endif
	pushq $\int_num
	jmp isr_common
.endm

# Exception handlers (32 exception handlers)

isr_exception_entry 0 0
isr_exception_entry 0 1
isr_exception_entry 0 2
isr_exception_entry 0 3
isr_exception_entry 0 4
isr_exception_entry 0 5
isr_exception_entry 0 6
isr_exception_entry 0 7
isr_exception_entry 1 8
isr_exception_entry 0 9
isr_exception_entry 1 10
isr_exception_entry 1 11
isr_exception_entry 1 12
isr_exception_entry 1 13
isr_exception_entry 1 14
isr_exception_entry 0 15
isr_exception_entry 0 16
isr_exception_entry 1 17
isr_exception_entry 0 18
isr_exception_entry 0 19
isr_exception_entry 0 20
isr_exception_entry 0 21
isr_exception_entry 0 22
isr_exception_entry 0 23
isr_exception_entry 0 24
isr_exception_entry 0 25
isr_exception_entry 0 26
isr_exception_entry 0 27
isr_exception_entry 0 28
isr_exception_entry 0 29
isr_exception_entry 1 30
isr_exception_entry 0 31

# PIC IRQ handlers (16 IRQs)

.irp int_num,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
	isr_entry 0 \int_num
.endr

# APIC handlers (24 IRQs)

.irp int_num,48,49,50,51,52,53,54,55
	isr_entry 0 \int_num
.endr
.irp int_num,56,57,58,59,60,61,62,63
	isr_entry 0 \int_num
.endr
.irp int_num,64,65,66,67,68,69,70,71
	isr_entry 0 \int_num
.endr

# Forced context switch entry point
isr_entry 0 72

# SMP APIC timer entry point
isr_entry 0 73

isr_common:
	# Save call-clobbered registers
	# (in System-V parameter order in memory)
	push %rax
	push %r11
	push %r10
	push %r9
	push %r8
	push %rcx
	push %rdx
	push %rsi
	push %rdi

	# Save FSBASE MSR
	movl $0xC0000100,%ecx
	rdmsr
	shl $32,%rdx
	or %rdx,%rax
	push %rax

	# Save segment registers
	sub $8,%rsp
	movw %gs,6(%rsp)
	movw %fs,4(%rsp)
	movw %es,2(%rsp)
	movw %ds,(%rsp)

	# Load segment registers with good 64-bit mode values
	mov $0x10,%ecx
	mov %ecx,%ds
	mov %ecx,%es
	mov %ecx,%fs
	# we don't care about gs in isr

	# Save pointer to general registers and return information
	mov %rsp,%rdi

	# 16 byte align stack and make room for fxsave
	and $-16,%rsp
	sub $512,%rsp

	# Save entire sse/fpu state
	fxsave (%rsp)

	# Make structure on the stack
	push %rsp
	push %rdi

	# Pass pointer to the context structure to isr_handler
	mov %rsp,%rdi
	call isr_handler

	# isr can return a new stack pointer, or just return
	# the passed one to continue with this thread
	mov %rax,%rsp

	# Pop the pointer to the general registers
	pop %rdi
	fxrstor 8(%rsp)

	mov %rdi,%rsp

	movw (%rsp),%ds
	movw 2(%rsp),%es
	movw 4(%rsp),%fs
	movw 6(%rsp),%gs
	add $8,%rsp

	# Restore FSBASE
	pop %rax
	mov %rax,%rdx
	shr $32,%rdx
	mov $0xC0000100,%ecx
	wrmsr

	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %r8
	pop %r9
	pop %r10
	pop %r11
	pop %rax

	addq $16,%rsp

	iretq

exception_common:
	# Save complete context
	push %r15
	push %r14
	push %r13
	push %r12
	push %r11
	push %r10
	push %r9
	push %r8
	push %rbp
	push %rdi
	push %rsi
	push %rdx
	push %rcx
	push %rbx
	push %rax

	# Save FSBASE MSR
	movl $0xC0000100,%ecx
	rdmsr
	shl $32,%rdx
	or %rdx,%rax
	push %rax

	sub $8,%rsp
	movw %gs,6(%rsp)
	movw %fs,4(%rsp)
	movw %es,2(%rsp)
	movw %ds,(%rsp)

	# Pointer to general register context in %rax
	mov %rsp,%rax

	# Load good segments
	mov $0x10,%ecx
	mov %ecx,%ds
	mov %ecx,%es
	mov %ecx,%fs

	# Align stack and make room for FPU/SSE context
	and $-16,%rsp
	sub $512,%rsp
	fxsave (%rsp)

	# Make structure with two pointers:
	#  general register context
	#  fxsave context
	push %rsp
	push %rax

	# Pass the structure
	# and a pointer to the interrupt number and error code
	mov %rsp,%rdi
	call exception_isr_handler

	# rax should point to isr_full_context_t here
	# it should be at the top of the stack just
	# in case an interrupt occurs while restoring
	# the context
	#mov %rax,%rsp

	#pop %rax
	#add $8,%rsp

	#fxrstor (%rsp)
	fxrstor 16(%rax)

	# Restore general registers
	mov (%rax),%rsp

	# Restore segments
	mov (%rsp),%ds
	mov 2(%rsp),%es
	mov 4(%rsp),%fs
	mov 6(%rsp),%gs
	add $8,%rsp

	# Restore FSBASE
	pop %rax
	mov %rax,%rdx
	shr $32,%rdx
	mov $0xC0000100,%ecx
	wrmsr

	pop %rax
	pop %rbx
	pop %rcx
	pop %rdx
	pop %rsi
	pop %rdi
	pop %rbp
	pop %r8
	pop %r9
	pop %r10
	pop %r11
	pop %r12
	pop %r13
	pop %r14
	pop %r15

	# Discard error code and interrupt number
	add $16,%rsp

	iretq
