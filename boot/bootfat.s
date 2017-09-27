.code16

# ptbl_ent
.struct 0
ptbl_ent_start:
ptbl_ent_bootflag:
	.struct ptbl_ent_bootflag + 1
ptbl_ent_sthead:
	.struct ptbl_ent_sthead + 1
ptbl_ent_stseccyl:
	.struct ptbl_ent_stseccyl + 2
ptbl_ent_sysid:
	.struct ptbl_ent_sysid + 1
ptbl_ent_enhead:
	.struct ptbl_ent_enhead + 1
ptbl_ent_enseccyl:
	.struct ptbl_ent_enseccyl + 2
ptbl_ent_stsec:
	.struct ptbl_ent_stsec + 4
ptbl_ent_numsec:
	.struct ptbl_ent_numsec + 4
ptbl_ent_end:
ptbl_ent_length = ptbl_ent_end - ptbl_ent_start

# dap
.struct 0
dap_start:
dap_sizeof_packet:
	.struct dap_sizeof_packet + 1
dap_reserved:
	.struct dap_reserved + 1
dap_block_count:
	.struct dap_block_count + 2
dap_address:
	.struct dap_address + 4
dap_lba:
	.struct dap_lba + 8
dap_end:
dap_length = dap_end - dap_start

# MBR calls this code with:
#  DS:SI -> partition table entry
#  DL = boot drive

.section .head
.globl entry
entry:
	jmp .+0x5A

.section .early
entry_start:
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
	pushw %si
	mov $512,%cx
	mov $0x7c00,%si
	mov $0x800,%di
	cld
	rep movsb
	popw %si

	ljmp $0,$reloc_entry
reloc_entry:

	#
	# Load rest of bootloader

	# Calculate number of sectors to load
	# (rounded up to a multiple of 512)
	movw $__initialized_data_end+511,%cx
	subw $first_sector_end,%cx
	shrw $9,%cx
	mov %cx,dap+dap_block_count

	# Get LBA of first sector after this one
	movl %fs:ptbl_ent_stsec(%si),%ecx
	incl %ecx
	movl %ecx,dap+dap_lba
	decl %ecx
disk_error:
	movw $0x4200,%ax
	mov $dap,%si
	int $0x13
	jc disk_error

	call clear_bss
	movb %dl,boot_drive

	movl %ecx,%eax
	pushl %eax

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

	popl %eax
	call fat32_boot_partition
unreachable:
	hlt
	jmp unreachable

dap:
	.byte dap_length
	.byte 0
	.word 1
	.long first_sector_end
	.quad 0
