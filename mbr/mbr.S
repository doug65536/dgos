.code16

#include "../boot/ptbl.h"
#include "../boot/dap.h"

#define UNRELOCATED(n) n+0x7C00-0x600

// BIOS calls this code with:
//  DL = boot drive
//  ES:DI -> PnP installation check stucture
//
// If the OS fails to boot, it should invoke INT 0x18

.text

// ----------------------------------------------------------------------
.globl entry
entry:
    ljmp $0,$ UNRELOCATED(entry_start)

init_stack:
    .int 0x0000FFF8

// ----------------------------------------------------------------------
// The bootstrap code starts here
.align 64
.globl entry_start
entry_start:
    // Incoming registers:
    //  DL = drive

    // Workaround for faulty BIOS, if DL is <= 0x80, use 0x80
    test %dl,%dl
    js 0f
    mov $0x80,%dl
0:

    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es
    
// hang and wait for gdb
//0:
//	test %ax,%ax
//	jz 0b

    // Initialize stack to top of first 64KB
    movw init_stack-0x600+0x7c00+2,%ss
    movw init_stack-0x600+0x7c00,%sp

    // Relocate to 0x600
    movw $0x7c00,%si
    movw $0x0600,%di
    movw $256,%cx
    cld
    rep movsw
    ljmpw $ 0,$ relocated_entry
relocated_entry:

    // Search for first bootable partition
    movw $partition_table,%si
    movw $4,%cx
0:
    cmpb $0x80,ptbl_ent_bootflag(%si)
    je found_partition
bad_signature:
    addw $ptbl_ent_length,%si
    decw %cx
    jnz 0b
    jmp no_active_partition

dap:
    .byte dap_length
    .byte 0
    .word 1
    .word 0x7c00
    .word 0x0000
    .quad 0

found_partition:
    pushw %si
    movw ptbl_ent_stsec(%si),%cx
    movw ptbl_ent_stsec+2(%si),%ax
    movw $dap,%si
    movw %cx,dap_lba(%si)
    movw %ax,dap_lba+2(%si)
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

// ----------------------------------------------------------------------
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
    // partition_table_entry 1,128,(1 << (24-9)),0x0C
    partition_table_entry_unused
    partition_table_entry_unused
    partition_table_entry_unused
    partition_table_entry_unused

    .byte 0x55
    .byte 0xAA
