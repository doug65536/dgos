.code32

.code16

# Note that this code goes out of its way to avoid using any
# instructions that will not work on an 8088

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
	jmp .+0x5C

// MBR calls this entry point with
//  dl = drive number
//  ds:si -> partition table entry
.section .early
entry_start:
	// Save pointer to partition table entry in registers
	movw %si,%bx
	movw %ds,%bp

	// Initialize segment registers
	xorw %ax,%ax
	movw %ax,%ds
	movw %ax,%es

	# Relocate to linked address
	movw $256,%cx
	movw $0x7c00,%si
	movw $entry,%di
	cld
	rep movsw

	// Save boot drive and pointer to partition entry
	movb %dl,boot_drive
	movw %bx,partition_entry_ptr
	movw %bp,2+partition_entry_ptr

	// Initialize stack pointer
	lssw initial_stack_ptr,%sp

	// Load cs register
	ljmpw $0,$reloc_entry
reloc_entry:

	#
	# Load rest of bootloader

	# Calculate number of sectors to load
	# (rounded up to a multiple of 512)
	movw $__initialized_data_end + 511,%cx
	subw $first_sector_end,%cx
	shrw $9,%cx
	movw %cx,dap+dap_block_count

	#
	# Get LBA of first sector after this one

	# Read 32-bit LBA from ds:si saved above
	ldsw partition_entry_ptr,%bx

	// Get partition start LBA into bp:di
	movw ptbl_ent_stsec(%bx),%di
	movw 2+ptbl_ent_stsec(%bx),%bp
	movw zerow,%ds

	# Add 1 to 32 bit LBA in bp:di
	addw $1,%di
	adcw $0,%bp

	# Store incremented LBA in DAP
	movw %di,dap+dap_lba
	movw %bp,2+dap+dap_lba

	# Restore original LBA in bp:di
	subw $1,%di
	sbbw $0,%bp

disk_read:
	movw $0x4200,%ax
	mov $dap,%si
	int $0x13
	jc disk_read

	call clear_bss

	call detect_ancient_cpu

	// 32 bit instructions are okay if detect_ancient_cpu returned...

	movzwl %sp,%esp

	// Copy bp:dp into eax
	movzwl %bp,%eax
	shll $16,%eax
	movw %di,%ax

	movl $fat32_boot_partition,%edx
	call boot
	cli
unreachable:
	hlt
	jmp unreachable

dap:
	.byte dap_length
	.byte 0
	.word 1
	.long first_sector_end
	.quad 0

.global boot_drive
boot_drive:
	.byte 0

.align 4
partition_entry_ptr:
	.int 0

initial_stack_ptr:
	.short __initial_stack
	.short 0

zerow:
	.short 0
