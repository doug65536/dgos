#include "dap.h"
#include "ptbl.h"

.code16

// Note that this code goes out of its way to avoid using any
// instructions that will not work on an 8088

// MBR calls this code with:
//  DS:SI -> partition table entry
//  DL = boot drive

.section .head
.global entry
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

	// Relocate to linked address
	movw $256,%cx
	movw $0x7c00,%si
	movw $entry,%di
	cld
	rep movsw

	// Save boot drive and pointer to partition entry
	movb %dl,boot_drive
	movw %bx,partition_entry_ptr
	movw %bp,2+partition_entry_ptr

	// Initialize stack pointer (lss is 386+ instruction)
	// Must load ss first to disable interrupts for one instruction
	movw initial_stack_ptr+2,%ss
	movw initial_stack_ptr,%sp

	call showplace

	// Load cs register
	ljmpw $0,$reloc_entry
reloc_entry:

	//
	// Load rest of bootloader

	// Calculate number of sectors to load
	// (rounded up to a multiple of 512)
	movw $___initialized_data_en + 511,%cx
	subw $first_sector_end,%cx
	shrw $9,%cx
	// cx holds remaining sectors...

	//
	// Get LBA of first sector after this one

	// Read 32-bit LBA from ds:si saved above (lds is 386+ instruction)
	movw partition_entry_ptr,%bx
	movw partition_entry_ptr+2,%ds

	// Get partition start LBA into bp:di (using ds loaded just above)
	movw ptbl_ent_stsec(%bx),%di
	movw 2+ptbl_ent_stsec(%bx),%bp

	// Reset ds
	xorw %ax,%ax
	movw %ax,%ds

	// Add 1 to 32 bit LBA in bp:di
	addw $1,%di
	adcw $0,%bp

	// Store incremented LBA in DAP
	movw %di,dap+dap_lba
	movw %bp,2+dap+dap_lba

	// Restore original LBA in bp:di
	subw $1,%di
	sbbw $0,%bp

disk_read:
	// Write the remaining block count to DAP
	movw %cx,dap+dap_block_count

	// Invoke LBA disk read
	movw $0x4200,%ax
	movw $dap,%si
	int $0x13

	jc disk_read

	// Read number of blocks actually read back from DAP
	movw dap+dap_block_count,%ax

	// Advance LBA
	addw %ax,dap+dap_lba
	adcw $0,dap+dap_lba+2

	// Reduce remaining block count and loop if any remaining
	subw %ax,%cx
	jnz disk_read

	call clear_bss

	call detect_ancient_cpu

	fninit

	// 32 bit instructions are okay if detect_ancient_cpu returned...

	movzwl %sp,%esp

	// Zero extend bp:dp into edx:eax
	movzwl %bp,%eax
	shll $16,%eax
	movw %di,%ax
	xor %edx,%edx

	movl $fat32_boot_partition,%ecx
	call boot
	cli
unreachable:
	hlt
	jmp unreachable

showplace:
	pushw %bp
	movw %sp,%bp
	pushaw
	pushfw

	pushw %es

	pushw $0xb800
	popw %es

	// xlat base
	movw $hexlookup,%bx

	movw 2(%bp),%dx
	xorw %di,%di

	// shift distance
	movb $12,%cl
.Lnibble_loop:
	movw %dx,%ax
	shrw %cl,%ax
	andb $0xF,%al
	xlat
	movb $7,%ah
	stosw
	sub $4,%cl
	jae .Lnibble_loop

	pop %es
	popfw
	popaw
	leavew
	ret

hexlookup:
.byte '0', '1', '2', '3', '4', '5', '6', '7'
.byte '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'

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
	.short ___initial_stack
	.short 0
