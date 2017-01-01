.text

.macro push_cfi val
	pushq \val
	.cfi_adjust_cfa_offset 8
.endm

.macro pop_cfi val
	popq \val
	.cfi_adjust_cfa_offset -8
.endm

.macro adj_rsp	ofs
	add $\ofs,%rsp
	.cfi_adjust_cfa_offset -(\ofs)
.endm

.macro isr_entry has_code int_num
.globl isr_entry_\int_num\()
.hidden isr_entry_\int_num\()
isr_entry_\int_num\():
	.cfi_startproc
	.if \has_code == 0
		.cfi_def_cfa_offset 8
		push_cfi $0
	.else
		.cfi_def_cfa_offset 16
	.endif
	push_cfi $\int_num
	jmp isr_common
	.cfi_endproc
.endm

# Exception handlers (32 exception handlers)

isr_entry 0 0
isr_entry 0 1
isr_entry 0 2
isr_entry 0 3
isr_entry 0 4
isr_entry 0 5
isr_entry 0 6
isr_entry 0 7
isr_entry 1 8
isr_entry 0 9
isr_entry 1 10
isr_entry 1 11
isr_entry 1 12
isr_entry 1 13
isr_entry 1 14
isr_entry 0 15
isr_entry 0 16
isr_entry 1 17
isr_entry 0 18
isr_entry 0 19
isr_entry 0 20
isr_entry 0 21
isr_entry 0 22
isr_entry 0 23
isr_entry 0 24
isr_entry 0 25
isr_entry 0 26
isr_entry 0 27
isr_entry 0 28
isr_entry 0 29
isr_entry 1 30
isr_entry 0 31


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

# Software interrupts
.irp int_num,72,73,74,75,76,77,78,79
	isr_entry 0 \int_num
.endr
.irp int_num,80,81,82,83,84,85,86,87
	isr_entry 0 \int_num
.endr
.irp int_num,88,89,90,91,92,93,94,95
	isr_entry 0 \int_num
.endr
.irp int_num,96,97,98,99,100,101,102,103
	isr_entry 0 \int_num
.endr
.irp int_num,104,105,106,107,108,109,110,111
	isr_entry 0 \int_num
.endr
.irp int_num,112,113,114,115,116,117,118,119
	isr_entry 0 \int_num
.endr
.irp int_num,120,121,122,123,124,125,126,127
	isr_entry 0 \int_num
.endr

isr_common:
	.cfi_startproc
	.cfi_def_cfa_offset 24

	# Save call-clobbered registers
	# (in System-V parameter order in memory)
	adj_rsp -120
	movq %rdi,    (%rsp)
	movq %rsi, 1*8(%rsp)
	movq %rdx, 2*8(%rsp)
	movq %rcx, 3*8(%rsp)
	movq %r8 , 4*8(%rsp)
	movq %r9 , 5*8(%rsp)
	movq %rax, 6*8(%rsp)
	movq %rbx, 7*8(%rsp)
	movq %rbp, 8*8(%rsp)
	movq %r10, 9*8(%rsp)
	movq %r11,10*8(%rsp)
	movq %r12,11*8(%rsp)
	movq %r13,12*8(%rsp)
	movq %r14,13*8(%rsp)
	movq %r15,14*8(%rsp)

	# C code requires that the direction flag is clear
	cld

	# See if we're coming from 64 bit code
	cmpq $8,18*8(%rsp)
	jne isr_save_32

	# ...yes, came from 64 bit code

	# Save FSBASE MSR
	movl $0xC0000100,%ecx
	rdmsr
	shl $32,%rdx
	or %rdx,%rax
	push_cfi %rax

	# Push segment registers
	movabs $0x0010001000100010,%rax
	push_cfi %rax

8:
	# Save pointer to general registers and return information
	mov %rsp,%rdi
	.cfi_register %rsp,%rdi

	# 16 byte align stack and make room for fxsave
	and $-16,%rsp
	adj_rsp -512

	# Save entire sse/mmx/fpu state
	fxsave (%rsp)

	# Make structure on the stack
	push_cfi %rsp
	push_cfi %rdi
	xor %eax,%eax
	push_cfi %rax
	push_cfi %rax

	# Pass pointer to the context structure to isr_handler
	mov %rsp,%rdi
	.cfi_register %rsp,%rdi
	call isr_handler

	# isr can return a new stack pointer, or just return
	# the passed one to continue with this thread
	mov %rax,%rsp

	# Pop outgoing cleanup data
	# Used to adjust outgoing thread state after switching stack
	pop_cfi %rax
	pop_cfi %rdi
	test %rax,%rax
	jz 0f
	call *%rax
0:

	# Pop the pointer to the general registers
	pop_cfi %rdi
	fxrstor 8(%rsp)

	mov %rdi,%rsp
	.cfi_register %rdi,%rsp

	# See if we're returning to 64 bit code
	cmp $8,20*8(%rsp)
	jnz isr_restore_32

	# ...yes, returning to 64 bit mode

	# Discard segments
	adj_rsp 8

	# Restore FSBASE
	pop_cfi %rax
	mov %rax,%rdx
	shr $32,%rdx
	mov $0xC0000100,%ecx
	wrmsr

6:
	movq     (%rsp),%rdi
	movq  1*8(%rsp),%rsi
	movq  2*8(%rsp),%rdx
	movq  3*8(%rsp),%rcx
	movq  4*8(%rsp),%r8
	movq  5*8(%rsp),%r9
	movq  6*8(%rsp),%rax
	movq  7*8(%rsp),%rbx
	movq  8*8(%rsp),%rbp
	movq  9*8(%rsp),%r10
	movq 10*8(%rsp),%r11
	movq 11*8(%rsp),%r12
	movq 12*8(%rsp),%r13
	movq 13*8(%rsp),%r14
	movq 14*8(%rsp),%r15

	addq $16+8*15,%rsp
	.cfi_def_cfa_offset 8

	iretq

# Saving context from 32 bit mode, out of line
isr_save_32:
	# Push dummy FSBASE
	push_cfi $0

	# Save 32 bit segment registers
	sub $8,%rsp
	movw %gs,6(%rsp)
	movw %fs,4(%rsp)
	movw %es,2(%rsp)
	movw %ds,(%rsp)

	# Get kernel mode GSBASE back
	swapgs

	jmp 8b

# Resuming into 32 bit mode, out of line
isr_restore_32:
	# Protect kernel mode gsbase from changes
	swapgs

	# Restore 32 bit segment registers
	movw (%rsp),%ds
	movw 2(%rsp),%es
	movw 4(%rsp),%fs
	movw 6(%rsp),%gs

	# Discard segments and FSBASE
	adj_rsp 16
	jmp 6b

	.cfi_endproc
