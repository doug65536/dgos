.code16
.section .head
.globl entry
entry:
	jmp entry_start

.org entry+8
.globl bootinfo_primary_volume_desc
bootinfo_primary_volume_desc:
.space 4

.globl bootinfo_file_location
bootinfo_file_location:
.space 4

.globl bootinfo_file_length
bootinfo_file_length:
.space 4

.globl bootinfo_checksum
bootinfo_checksum:
.space 4

.globl bootinfo_reserved
bootinfo_reserved:
.space 10*4

.section .early
entry_start:
	fninit

	# Initialize stack
	xorw %ax,%ax
	pushw %ax
	pushw $__initial_stack
	movw %sp,%bp
	lss (%bp),%sp
	movw %sp,%bp

	# Save segment of partition table entry in fs
	movw %ds,%cx
	movw %cx,%fs

	# Initialize segment registers
	movw %ax,%ds
	movw %ax,%es

	# Relocate to 0x800
	mov $__initialized_data_end,%cx
	sub $entry,%cx
	mov $0x7c00,%si
	mov $0x800,%di
	cld
	rep movsb

	ljmp $0,$reloc_entry
reloc_entry:
	call clear_bss
	mov %dl,boot_drive

call_constructors:
	movl $__ctors_start,%ebx
0:
	cmpl $__ctors_end,%ebx
	jae 0f
	movl (%ebx),%eax
	test %eax,%eax
	jz 1f
	cmpl $entry,%eax
	jb 1f
	call *%eax
1:
	addl $4,%ebx
	jmp 0b
0:

	mov bootinfo_primary_volume_desc,%eax
	call iso9660_boot_partition
unreachable:
	hlt
	jmp unreachable

.global boot_drive
boot_drive:
	.byte 0
