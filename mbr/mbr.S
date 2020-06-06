.code16

#include "../boot/ptbl.h"
#include "../boot/gpt.h"
#include "../boot/dap.h"

// Code is linked at 0x600, but initially loaded at 0x7c00
// Helper to adjust symbols to where they are before relocating to 0x600
#define UNRELOCATED(n) (0x7c00-0x600)+n

// BIOS calls this code with:
//  DL = boot drive
//  ES:DI -> PnP installation check stucture
//
// If the OS fails to boot, it should invoke INT 0x18

.text

// ----------------------------------------------------------------------
.globl entry
entry:
    ljmp $ 0,$ UNRELOCATED(entry_start)

init_stack:
    .int 0x0000FFF8

dap:
    .byte dap_length
    .byte 0
    .word 1
    .word 0x7c00
    .word 0x0000
    .quad 0

ptbl_ent_ptr:
    .word 0

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
    mov $ 0x80,%dl
0:

    // Don't clobber dx!

    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es

    // Initialize stack to top of first 64KB
    // Could do this: lss UNRELOCATED(init_stack),%sp
    // but I want code running this early to be 8088 compatible
    movw UNRELOCATED(init_stack+2),%ss
    movw UNRELOCATED(init_stack),%sp

    // Relocate to 0x600
    movw $ 0x7c00,%si
    movw $ 0x0600,%di
    movw $ 256,%cx
    cld
    rep movsw
    ljmpw $ 0,$ relocated_entry
relocated_entry:

    // Search for first bootable partition
    movw $ partition_table,%bx
    movw $ 4,%cx

0:  cmpb $ 0xEE,ptbl_ent_sysid(%bx)
    je found_gpt

    cmpb $ 0x80,ptbl_ent_bootflag(%bx)
    je boot_partition

bad_signature:
    addw $ ptbl_ent_length,%bx
    decw %cx
    jnz 0b
    jmp no_active_partition

boot_partition:
    movw ptbl_ent_stsec(%bx),%ax
    movw ptbl_ent_stsec+2(%bx),%cx
    movw $ 1,dap+dap_block_count
    call disk_io_with_lba
    cmpw $ 0xaa55,0x7dfe
    je 0x7c00
    jmp bad_signature

found_gpt:
    movw %bx,ptbl_ent_ptr
    movw $ 2,dap+dap_lba
    andw $ 0,dap+dap_lba+2

.Lanother_gpt:
    movw $ 1,dap+dap_block_count
    call disk_io

    // Look for bootable flag
    testb $ gpt_lba2_part_attr_bios,0x7c00+gpt_lba2_part_attr
    jnz boot_gpt

    addw $ 1,dap+dap_lba
    adcw $ 0,dap+dap_lba+2
    jmp .Lanother_gpt

boot_gpt:
    // Calculate 32 bit sector count (2TB limit)
    movw 0x7c00+gpt_lba2_part_lba_en,%ax
    movw 0x7c00+gpt_lba2_part_lba_en+2,%cx
    subw 0x7c00+gpt_lba2_part_lba_st,%ax
    sbbw 0x7c00+gpt_lba2_part_lba_st+2,%cx
    addw $ 1,%ax
    adcw $ 0,%cx
    movw %ax,ptbl_ent_numsec(%bx)
    movw %cx,ptbl_ent_numsec+2(%bx)

    movw 0x7c00+gpt_lba2_part_lba_st,%ax
    movw 0x7c00+gpt_lba2_part_lba_st+2,%cx
    movw %ax,ptbl_ent_stsec(%bx)
    movw %cx,ptbl_ent_stsec+2(%bx)
    jmp boot_partition

.Lpart_no_good:
    incb dap_lba(%si)
    cmpb $ 34,dap_lba(%si)
    jb .Lanother_gpt

//.space 220

disk_io_with_lba:
    movw %ax,dap_lba(%si)
    movw %cx,dap_lba+2(%si)
    // fall through
disk_io:
    movw $ dap,%si
    movw $ 0x4200,%ax
    int $ 0x13
    jc disk_error
    ret

disk_error:
    movw $ 'd'+0xF00,%ax
    jmp fail_message

bad_bootsector:
    movw $ 'b'+0xF00,%ax
    jmp fail_message

no_active_partition:
    movw $ 'p'+0xF00,%ax

fail_message:
    pushw $ 0xb800
    popw %ds
    movw %ax,0

give_up:
    int $ 0x18
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
