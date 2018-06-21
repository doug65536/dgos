#include "dap.h"
#include "ptbl.h"

.code16

// Note that this code goes out of its way to avoid using any
// instructions that will not work on an 8088

// stage1 calls this code with:
//  DS:SI -> start LBA of partition
//  DL = boot drive

// MBR calls this entry point with
//  dl = drive number
//  ds:si -> partition table entry

.section .early
.global entry
entry:
    mov %dl,boot_drive
    lss initial_stack_ptr,%sp
    
    call clear_bss

	call detect_ancient_cpu

	fninit

	// 32 bit instructions are okay if detect_ancient_cpu returned...

	movzwl %sp,%esp

	// Load partition start LBA into edx:eax
	movl ptbl_ent_stsec(%si),%eax
    xor %edx,%edx

	movl $fat32_boot_partition,%ecx
	call boot
	cli
unreachable:
	hlt
	jmp unreachable

.global boot_drive
boot_drive:
	.byte 0

initial_stack_ptr:
	.word ___initial_stack
	.word 0
