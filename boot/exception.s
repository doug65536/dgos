.code64

.section .low, "ax", @progbits

.macro isr_entry has_code int_num
.global isr_entry_\int_num\()
isr_entry_\int_num\():
	.if \has_code == 0
		pushq $ 0
	.endif
	pushq $ \int_num
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
	# Save registers
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

	# Save segment registers
	sub $ 8,%rsp
	movw %ds,(%rsp)
	movw %es,2(%rsp)
	movw %fs,4(%rsp)
	movw %gs,6(%rsp)

	# Pass a pointer to the interrupt number and error code
	mov %rsp,%r10
	call isr_handler

	movw (%rsp),%ds
	movw 2(%rsp),%es
	movw 4(%rsp),%fs
	movw 6(%rsp),%gs

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

	addq $ 16,%rsp

	iretq

.balign 8

msg_exception:
	.string "Exception=0x"

msg_code:
	.string " Code=0x"

msg_address:
	.string " Addr=0x"

msg_CR2:
	.string " CR2=0x"

isr_handler:
	# Black on red
	movb $ 0x0C,%ah
	# Text video memory
	movl $ 0xb8000,%edi

	# Exception=
	movl $ msg_exception,%esi
	call text_out

	# Exception number
	movq 16*8(%r10),%rsi
	call hex2_out

	# Code=
	movl $ msg_code,%esi
	call text_out

	# Error code
	movq 17*8(%r10),%rsi
	call hex8_out

	# Addr=
	movl $ msg_address,%esi
	call text_out

	# Address
	movq 18*8(%r10),%rsi
	call hex16_out

	# CR2=

	movl $ msg_CR2,%esi
	call text_out

	# Page fault address
	mov %cr2,%rsi
	call hex16_out

	# Next line
	movl $ 0xb8000 + 80*2,%edi

	# Name of exception
	movzbq 16*8(%r10),%r12
	movl $ isr_name_invalid,%esi
	cmpl $ 32,%r12d
	ja 0f
	movl $ isr_names,%r11d
	movzwl (%r11,%r12,2),%esi
0:	call text_out

	# Registers

	xor %r13d,%r13d

	# Line
1:	imul $ 160,%r13d,%edi
	add $ 0xb8000 + 80*4,%edi

	# Register name
	movl $ reg_names,%r14d
	lea (%r14,%r13,4),%rsi
	call text_out

	mov $ equal_separator,%esi
	call text_out

	# Register value
	mov 8(%r10,%r13,8),%rsi
	call hex16_out

	inc %r13d
	cmp $ 14,%r13d
	jb 1b


	# Undocumented instruction "icebp"
0:	.byte 0xf1
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
	push %rax
	in $ 0xe9,%al
	cmp $ 0xe9,%al
	pop %rax
	jnz text_out
	out %al,$ 0xe9
	jmp text_out
0:	ret

# print 2 digit hex (8 bits), see hex_common
hex2_out:
	shl $ 64-8,%rsi
	mov $ 2,%r8b
	jmp hex_common

# print 8 digit hex (16 bits), see hex_common
hex8_out:
	shl $ 64-32,%rsi
	mov $ 8,%r8b
	jmp hex_common

hex16_out:
	mov $ 16,%r8b

#  in: %ah=attribute, %rsi=number, %rdi=video memory
#  in: %r8b number of hex digits
#      number must be in highest bits of rsi
# out: %rsi=0, %rdi=advanced output pointer %r9=hexlookup
#      %al=0
# clobber: %rdx %r8b
hex_common:
	mov $ hexlookup,%r9d
0:	xorq %rdx,%rdx
	shld $ 4,%rsi,%rdx
	shl $ 4,%rsi
	movb (%r9,%rdx,1),%al
	movw %ax,(%rdi)
	push %rax
	in $ 0xe9,%al
	cmp $ 0xe9,%al
	pop %rax
	jnz 0f
	out %al,$ 0xe9
0:	leaq 2(%rdi),%rdi
	decb %r8b
	jnz 0b
	ret

# Table for populating IDT
.global isr_table
isr_table:
.hword isr_entry_0
.hword isr_entry_1
.hword isr_entry_2
.hword isr_entry_3
.hword isr_entry_4
.hword isr_entry_5
.hword isr_entry_6
.hword isr_entry_7
.hword isr_entry_8
.hword isr_entry_9
.hword isr_entry_10
.hword isr_entry_11
.hword isr_entry_12
.hword isr_entry_13
.hword isr_entry_14
.hword isr_entry_15
.hword isr_entry_16
.hword isr_entry_17
.hword isr_entry_18
.hword isr_entry_19
.hword isr_entry_20
.hword isr_entry_21
.hword isr_entry_22
.hword isr_entry_23
.hword isr_entry_24
.hword isr_entry_25
.hword isr_entry_26
.hword isr_entry_27
.hword isr_entry_28
.hword isr_entry_29
.hword isr_entry_30
.hword isr_entry_31

isr_name_invalid: .string "(Invalid!)"
isr_name_reserved: .string "(Reserved)"
isr_name_0:  .string "#DE Divide Error"
isr_name_1:  .string "#DB Debug"
isr_name_2:  .string "NMI"
isr_name_3:  .string "#BP Breakpoint"
isr_name_4:  .string "#OF Overflow"
isr_name_5:  .string "#BR BOUND Range Exceeded"
isr_name_6:  .string "#UD Invalid Opcode"
isr_name_7:  .string "#NM Device Not Available"
isr_name_8:  .string "#DF Double Fault"
isr_name_10: .string "#TS Invalid TSS"
isr_name_11: .string "#NP Segment Not Present"
isr_name_12: .string "#SS Stack Fault"
isr_name_13: .string "#GP General Protection"
isr_name_14: .string "#PF Page Fault"
isr_name_16: .string "#MF Floating-Point Error"
isr_name_17: .string "#AC Alignment Check"
isr_name_18: .string "#MC Machine Check"
isr_name_19: .string "#XM SIMD"
isr_name_20: .string "#VE Virtualization"

.global isr_names
isr_names:
.hword isr_name_0
.hword isr_name_1
.hword isr_name_2
.hword isr_name_3
.hword isr_name_4
.hword isr_name_5
.hword isr_name_6
.hword isr_name_reserved
.hword isr_name_8
.hword isr_name_reserved
.hword isr_name_10
.hword isr_name_11
.hword isr_name_12
.hword isr_name_13
.hword isr_name_14
.hword isr_name_reserved
.hword isr_name_16
.hword isr_name_17
.hword isr_name_18
.hword isr_name_19
.hword isr_name_20
.hword isr_name_reserved
.hword isr_name_reserved
.hword isr_name_reserved
.hword isr_name_reserved
.hword isr_name_reserved
.hword isr_name_reserved
.hword isr_name_reserved
.hword isr_name_reserved
.hword isr_name_reserved
.hword isr_name_reserved
.hword isr_name_reserved

# Register names
reg_names:
.string "rax"
.string "rbx"
.string "rcx"
.string "rdx"
.string "rsi"
.string "rdi"
.string "rbp"
.string "r8 "
.string "r9 "
.string "r10"
.string "r12"
.string "r13"
.string "r14"
.string "r15"
equal_separator:
.string "="
