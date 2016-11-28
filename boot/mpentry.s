.code16
.section ".mp_entry"

# This must be position independent code
.globl mp_entry
.globl mp_entry_size
mp_entry:
	movl mp_enter_kernel,%eax
	movl mp_enter_kernel+4,%edx
	pushl $0f
	ljmp $0x0000,$enter_kernel
0:
    hlt
    jmp 0b

.align 4
mp_entry_size:
	.int mp_entry_size - mp_entry
