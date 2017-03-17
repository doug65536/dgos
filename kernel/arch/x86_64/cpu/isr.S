.section .text, "ax"

.macro push_cfi val
	pushq \val
#	.cfi_adjust_cfa_offset 8
.endm

.macro pop_cfi val
	popq \val
#	.cfi_adjust_cfa_offset -8
.endm

.macro add_rsp	ofs
	add \ofs,%rsp
#	.cfi_adjust_cfa_offset -(\ofs)
.endm

.macro sub_rsp	ofs
	sub \ofs,%rsp
#	.cfi_adjust_cfa_offset -(\ofs)
.endm

.macro isr_entry has_code int_num
.global isr_entry_\int_num\()
.hidden isr_entry_\int_num\()
.align 16
isr_entry_\int_num\():
#	.cfi_startproc
	.if \has_code == 0
#		.cfi_def_cfa_offset 8
		push_cfi $0
	.else
#		.cfi_def_cfa_offset 16
	.endif
	push_cfi $\int_num
	jmp isr_common
#	.cfi_endproc
.endm

# Software interrupts
# 72 thru 74 moved close
.irp int_num,75,76,77,78,79
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
.irp int_num,128,129,130,131,132,133,134,135
	isr_entry 0 \int_num
.endr
.irp int_num,136,137,138,139,140,141,142,143
	isr_entry 0 \int_num
.endr
.irp int_num,144,145,146,147,148,149,150,151
	isr_entry 0 \int_num
.endr
.irp int_num,152,153,154,155,156,157,158,159
	isr_entry 0 \int_num
.endr
.irp int_num,160,161,162,163,164,165,166,167
	isr_entry 0 \int_num
.endr
.irp int_num,168,169,170,171,172,173,174,175
	isr_entry 0 \int_num
.endr
.irp int_num,176,177,178,179,180,181,182,183
	isr_entry 0 \int_num
.endr
.irp int_num,184,185,186,187,188,189,190,191
	isr_entry 0 \int_num
.endr
.irp int_num,192,193,194,195,196,197,198,199
	isr_entry 0 \int_num
.endr
.irp int_num,200,201,202,203,204,205,206,207
	isr_entry 0 \int_num
.endr
.irp int_num,208,209,210,211,212,213,214,215
	isr_entry 0 \int_num
.endr
.irp int_num,216,217,218,219,220,221,222,223
	isr_entry 0 \int_num
.endr
.irp int_num,224,225,226,227,228,229,230,231
	isr_entry 0 \int_num
.endr
.irp int_num,232,233,234,235,236,237,238,239
	isr_entry 0 \int_num
.endr
.irp int_num,240,241,242,243,244,245,246,247
	isr_entry 0 \int_num
.endr
.irp int_num,248,249,250,251,252,253,254,255
	isr_entry 0 \int_num
.endr

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
# 14 moved close to handler
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
.irp int_num,64,65,66,67,68,69,70,71
	isr_entry 0 \int_num
.endr
.irp int_num,56,57,58,59,60,61,62,63
	isr_entry 0 \int_num
.endr
.irp int_num,48,49,50,51,52,53,54,55
	isr_entry 0 \int_num
.endr

.irp int_num,72,73,74
	isr_entry 0 \int_num
.endr

isr_entry 1 14

.align 16
isr_common:
#	.cfi_startproc
#	.cfi_def_cfa_offset 24

	# Save call-clobbered registers
	# (in System-V parameter order in memory)
	push_cfi %r15
	push_cfi %r14
	push_cfi %r13
	push_cfi %r12
	push_cfi %r11
	push_cfi %r10
	push_cfi %rbp
	push_cfi %rbx
	push_cfi %rax
	push_cfi %r9
	push_cfi %r8
	push_cfi %rcx
	push_cfi %rdx
	push_cfi %rsi
	push_cfi %rdi

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

	# Really store segment registers if it is a #GP fault
	cmpl $0x0D,16*8(%rsp)
	jz isr_save_real_segs

	# Push segment registers
	movabs $0x0010001000100010,%rax
	push_cfi %rax

	.align 16
8:
	# Save pointer to general registers and return information
	mov %rsp,%rdi
#	.cfi_register %rsp,%rdi

	# Save entire sse/mmx/fpu state
	jmp *sse_context_save
isr_save_done:

	# Make structure on the stack
	push_cfi %rsp
	push_cfi %rdi
	xor %eax,%eax
	push_cfi %rax
	push_cfi %rax

	# Pass pointer to the context structure to isr_handler
	mov %rsp,%rdi
#	.cfi_register %rsp,%rdi
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
	jmp *sse_context_restore
isr_restore_done:

	mov %rdi,%rsp
#	.cfi_register %rdi,%rsp

	# See if we're returning to 64 bit code
	cmpl $8,20*8(%rsp)
	jnz isr_restore_32

	# ...yes, returning to 64 bit mode

	# Discard segments
	add_rsp $8

	# Restore FSBASE
	pop_cfi %rax
	mov %rax,%rdx
	shr $32,%rdx
	mov $0xC0000100,%ecx
	wrmsr

6:
	pop_cfi %rdi
	pop_cfi %rsi
	pop_cfi %rdx
	pop_cfi %rcx
	pop_cfi %r8
	pop_cfi %r9
	pop_cfi %rax
	pop_cfi %rbx
	pop_cfi %rbp
	pop_cfi %r10
	pop_cfi %r11
	pop_cfi %r12
	pop_cfi %r13
	pop_cfi %r14
	pop_cfi %r15

	addq $16,%rsp
#	.cfi_def_cfa_offset 8

	iretq

.align 16
isr_save_real_segs:
	sub $8,%rsp
	mov %gs,6(%rsp)
	mov %fs,4(%rsp)
	mov %es,2(%rsp)
	mov %ds,(%rsp)
	jmp 8b

# Saving context from 32 bit mode, out of line
.align 16
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
.align 16
isr_restore_32:
	# Protect kernel mode gsbase from changes
	swapgs

	# Restore 32 bit segment registers
	movw (%rsp),%ds
	movw 2(%rsp),%es
	movw 4(%rsp),%fs
	movw 6(%rsp),%gs

	# Discard segments and FSBASE
	add_rsp $16
	jmp 6b

#	.cfi_endproc

.align 16
.global isr_save_fxsave
.hidden isr_save_fxsave
isr_save_fxsave:
	# 16 byte align stack and make room for fxsave
	and $-16,%rsp
	sub_rsp 512
	fxsave64 (%rsp)
	jmp isr_save_done

.align 16
.global isr_restore_fxrstor
.hidden isr_restore_fxrstor
isr_restore_fxrstor:
	fxrstor64 8(%rsp)
	jmp isr_restore_done

.align 16
.global isr_save_xsave
.hidden isr_save_xsave
isr_save_xsave:
	# 64 byte align stack and make room for fxsave
	sub_rsp sse_context_size
	and $-64,%rsp

	# Clear XSAVE header
	xor %eax,%eax
	mov $512,%ecx
	mov %rax,0*8(%rsp,%rcx)
	mov %rax,1*8(%rsp,%rcx)
	mov %rax,2*8(%rsp,%rcx)
	mov %rax,3*8(%rsp,%rcx)
	mov %rax,4*8(%rsp,%rcx)
	mov %rax,5*8(%rsp,%rcx)
	mov %rax,6*8(%rsp,%rcx)
	mov %rax,7*8(%rsp,%rcx)

	# Save all enabled states
	mov $-1,%eax
	mov $-1,%edx
	xsave64 (%rsp)
	jmp isr_save_done

.align 16
.global isr_save_xsavec
.hidden isr_save_xsavec
isr_save_xsavec:
	# 64 byte align stack and make room for fxsave
	sub_rsp sse_context_size
	and $-64,%rsp

	# Clear XSAVE header
	xor %eax,%eax
	mov $512,%ecx
	mov %rax,0*8(%rsp,%rcx)
	mov %rax,1*8(%rsp,%rcx)
	mov %rax,2*8(%rsp,%rcx)
	mov %rax,3*8(%rsp,%rcx)
	mov %rax,4*8(%rsp,%rcx)
	mov %rax,5*8(%rsp,%rcx)
	mov %rax,6*8(%rsp,%rcx)
	mov %rax,7*8(%rsp,%rcx)

	# Save all enabled states in compact format
	mov $-1,%eax
	mov $-1,%edx
	xsavec64 (%rsp)
	jmp isr_save_done

.align 16
.global isr_restore_xrstor
.hidden isr_restore_xrstor
isr_restore_xrstor:
	mov $-1,%eax
	mov $-1,%edx
	xrstor64 8(%rsp)
	jmp isr_restore_done

