.code16

# ----------------------------------------------------------------------
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

# ----------------------------------------------------------------------
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

.text

# ----------------------------------------------------------------------
.globl entry
entry:
	ljmp $0,$entry_start+(0x7c00-0x600)

#
# CDROM boot requires this space to be allocated in the boot sector
# The CD mastering tool populates it with information
#

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

init_stack:
	.int 0x0000FFF8

# ----------------------------------------------------------------------
# The bootstrap code starts here
.globl entry_start
entry_start:
	# Incoming registers:
	#  DL = drive

	xorw %ax,%ax
	movw %ax,%ds
	movw %ax,%es

	# Initialize stack to top of first 64KB
	cli
	movw init_stack-0x600+0x7c00+2,%ss
	movw init_stack-0x600+0x7c00,%sp
	sti

	# Relocate to 0x600
	movw $0x7c00,%si
	movw $0x0600,%di
	movw $256,%cx
	cld
	rep movsw
	ljmp $0,$relocated_entry
relocated_entry:

	# Search for first bootable partition
	movw $partition_table,%si
	movw $4,%cx
0:
	cmpb $0x80,ptbl_ent_bootflag(%si)
	je found_partition
bad_signature:
	addw $ptbl_ent_length,%si
	dec %cx
	jnz 0b
	jmp no_active_partition

dap:
	.byte dap_length
	.byte 0
	.word 1
	.long 0x7c00
	.quad 0

found_partition:
	pushw %si
	movl ptbl_ent_stsec(%si),%ecx
	movw $dap,%si
	movl %ecx,dap_lba(%si)
	movw $0x4200,%ax
	int $0x13
	jc disk_error
	popw %si
	cmpw $0xaa55,0x7dfe
	je 0x7c00
	jmp bad_signature

disk_error:
	movw $'d'+0xF00,%ax
	jmp fail_message

bad_bootsector:
	movw $'b'+0xF00,%ax
	jmp fail_message

no_active_partition:
	movw $'p'+0xF00,%ax

fail_message:
	pushw $0xb800
	popw %ds
	movw %ax,0

give_up:
	int $0x18
	jmp give_up

# ----------------------------------------------------------------------
.section .parttab

.macro partition_table_entry active,lba_start,lba_sectors,type
	.if \active != 0
		.byte 0x80
	.else
		.byte 0x00
	.endif

	.byte 0
	.word 0

	.byte \type

	.byte 0
	.word 0

	.int \lba_start
	.int \lba_sectors
.endm

.macro partition_table_entry_unused
	.byte 0
	.byte 0
	.word 0
	.byte 0
	.byte 0
	.word 0
	.int 0
	.int 0
.endm

partition_table:
	# partition_table_entry 1,128,(1 << (24-9)),0x0C
	partition_table_entry_unused
	partition_table_entry_unused
	partition_table_entry_unused
	partition_table_entry_unused

	.byte 0x55
	.byte 0xAA
