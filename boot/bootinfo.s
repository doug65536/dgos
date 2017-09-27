.code16
.section .text
.globl halt
halt:
	mov $0xb800,%cx
	mov %cx,%es
	mov %ax,%si
	xor %di,%di
	mov $0xF,%ah
0:
	lodsb
	test %al,%al
	jz 0f
	stosw
	jmp 0b
0:
	hlt
	jmp 0b

.globl clear_bss
clear_bss:
	pushw %di
	pushw %cx
	pushw %ax
	movw $__bss_start,%di
	movw $__bss_end,%cx
	subw %di,%cx
	xorb %al,%al
	cld
	rep
	stosb
	popw %ax
	popw %cx
	popw %di
	ret

.section .bootinfo

.globl mp_entry_vector
mp_entry_vector:
	.int 0

.globl vbe_info_vector
vbe_info_vector:
	.int 0

.globl bootdev_info_vector
bootdev_info_vector:
	.int 0

.section .parttab
	.space 16*4
	.word 0xAA55

.globl first_sector_end
first_sector_end:
