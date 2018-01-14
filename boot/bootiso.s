.code32

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

	# Relocate to linked address
	movw $256,%cx
	movw $0x7c00,%si
	movw $entry,%di
	cld
	rep movsw

	ljmpw $0,$reloc_entry
reloc_entry:
	mov %dl,boot_drive
	call clear_bss

	mov bootinfo_primary_volume_desc,%eax
	xor %edx,%edx
	mov $iso9660_boot_partition,%ecx
	call boot
unreachable:
	hlt
	jmp unreachable

.global boot_drive
boot_drive:
	.byte 0
