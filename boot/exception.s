.code64

.section .text.unlikely

.macro isr_entry has_code int_num
.globl isr_entry_\int_num\()
isr_entry_\int_num\():
	.if \has_code == 0
		pushq $0
	.endif
	pushq $\int_num
	jmp isr_common
.endm

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

isr_common:
	# Save call-clobbered registers
	push %rax
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11

	# Save segment registers
	movl %ds,%eax
	push %rax
	movl %ds,%eax
	push %rax
	movl %es,%eax
	push %rax
	movl %fs,%eax
	push %rax

	# Load segment registers with good values
	movl $0x10,%eax
	movl %eax,%ds
	movl %eax,%es
	movl %eax,%fs

	# Pass a pointer to the
	leaq 13*8(%rsp),%rax
	push %rax
	call isr_handler
	pop %rax

	pop %rax
	mov %eax,%fs
	pop %rax
	mov %eax,%es
	pop %rax
	mov %eax,%ds

	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %rax

	addq $16,%rsp

	iret

.align 8

msg_exception:
	.string "Exception="

msg_code:
	.string " Code="

msg_address:
	.string " Addr="

isr_handler:
	movq 8(%rsp),%r10

	# Black on red
	movb $0x0C,%ah
	# Text video memory
	movl $0xb8000,%edi

	# Exception=
	movl $msg_exception,%esi
	call text_out

	# Exception number
	movq 0(%r10),%rsi
	call hex_out

	# Code=
	movl $msg_code,%esi
	call text_out

	# Error code
	movq 8(%r10),%rsi
	call hex_out

	# Code=
	movl $msg_address,%esi
	call text_out

	# Address
	movq 16(%r10),%rsi
	call hex_out

0:
	hlt
	jmp 0b

#  in: %ah=attribute, %rsi=string, %rdi=video memory
# out: %rsi=end of string, %rdi=advanced output pointer
#      %al=0
text_out:
	movb (%rsi),%al
	incq %rsi
	test %al,%al
	jz 0f
	movw %ax,(%rdi)
	leaq 2(%rdi),%rdi
	jmp text_out
0:
	ret

#  in: %ah=attribute, %rsi=number, %rdi=video memory
# out: %rsi=0, %rdi=advanced output pointer %r9=hexlookup
#      %al=0
# clobber: %rdx %r8b
hex_out:
	mov $16,%r8b
	mov $hexlookup,%r9d
0:
	xorq %rdx,%rdx
	shld $4,%rsi,%rdx
	shl $4,%rsi
	movb (%r9,%rdx,1),%al
	movw %ax,(%rdi)
	leaq 2(%rdi),%rdi
	decb %r8b
	jnz 0b
	ret

# Table for populating IDT
.globl isr_table
isr_table:
.word isr_entry_0
.word isr_entry_1
.word isr_entry_2
.word isr_entry_3
.word isr_entry_4
.word isr_entry_5
.word isr_entry_6
.word isr_entry_7
.word isr_entry_8
.word isr_entry_9
.word isr_entry_10
.word isr_entry_11
.word isr_entry_12
.word isr_entry_13
.word isr_entry_14
.word isr_entry_15
.word isr_entry_16
.word isr_entry_17
.word isr_entry_18
.word isr_entry_19
.word isr_entry_20
.word isr_entry_21
.word isr_entry_22
.word isr_entry_23
.word isr_entry_24
.word isr_entry_25
.word isr_entry_26
.word isr_entry_27
.word isr_entry_28
.word isr_entry_29
.word isr_entry_30
.word isr_entry_31
