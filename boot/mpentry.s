.code16
.text

# This must be position independent code
.globl mp_entry
.globl mp_entry_size
mp_entry:
	cli
	cld
	xor %ax,%ax
	movw %ax,%ds
	movw %ax,%es
	movw %ax,%ss
	movl $0xFFF0,%esp
	movl mp_enter_kernel,%eax
	movl mp_enter_kernel+4,%edx
	ljmp $0x0000,$enter_kernel
0:
	cli
	hlt
	jmp 0b

.align 8
mp_entry_size:
	.int mp_entry_size - mp_entry
