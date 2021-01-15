.code16

#include "../boot/ptbl.h"
#include "../boot/gpt.h"
#include "../boot/x86_bios/dap.h"

// Code is linked at 0x500, but initially loaded at 0x7c00
// Helper to adjust symbols to where they are before relocating to 0x500
#define RELOC_OFS   0x0500
#define LOAD_OFS    0x7C00
#define UNRELOCATED(n) (n+LOAD_OFS-RELOC_OFS)

// BIOS calls this code with:
//  DL = boot drive
//  ES:DI -> PnP installation check stucture
//
// If the OS fails to boot, it should invoke INT 0x18

.section .mbrtext, "ax", @progbits

// ----------------------------------------------------------------------
.global mbr_entry
mbr_entry:
    // Loads cs with 0 if the MBR jumped to 0x7c0:0 instead of 0:0x7c00
    // Jump over
    ljmpw $ 0,$ UNRELOCATED(.Lload_cs)

.Lload_cs:
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

    // Initialize stack to second 64KB
    // Could do this: lss UNRELOCATED(init_stack),%sp
    // but I want code running this early to be 8088 compatible
    movw UNRELOCATED(mbr_init_stack+2),%ss
    movw UNRELOCATED(mbr_init_stack),%sp

    // Relocate to 0x500
    movw $ LOAD_OFS,%si
    movw $ RELOC_OFS,%di
    movw $ 256,%cx
    cld
    rep movsw
    ljmpw $ 0,$ mbr_relocated_entry
mbr_relocated_entry:

    // Search for first bootable partition
    movw $ mbr_partition_table,%bx
    movw $ 4,%cx

.Lentry_loop:
    cmpb $ 0xEE,ptbl_ent_sysid(%bx)
    je mbr_found_gpt

    cmpb $ 0x80,ptbl_ent_bootflag(%bx)
    jne mbr_bad_signature

mbr_boot_partition:
    movw ptbl_ent_stsec(%bx),%ax
    movw ptbl_ent_stsec+2(%bx),%cx
    movw $ 1,mbr_dap+dap_block_count
    call mbr_disk_io_with_lba

    cmpw $ 0xaa55,0x7dfe
    jne mbr_bad_signature

    // Pass pointer to booted partition table entry
    mov %bx,%si
    // dl never changed yet
    jmp 0x7c00

mbr_bad_signature:
    addw $ ptbl_ent_length,%bx
    decw %cx
    jnz .Lentry_loop
    jmp mbr_no_active_partition

mbr_found_gpt:
    movw %bx,mbr_ptbl_ent_ptr
    movw $ 2,mbr_dap+dap_lba
    andw $ 0,mbr_dap+dap_lba+2

.Lanother_gpt:
    movw $ 1,mbr_dap+dap_block_count
    call mbr_disk_io

    // Look for bootable flag
    testb $ gpt_lba2_part_attr_bios,0x7c00+gpt_lba2_part_attr
    jnz mbr_boot_gpt

    addw $ 1,mbr_dap+dap_lba
    adcw $ 0,mbr_dap+dap_lba+2
    jmp .Lanother_gpt

mbr_boot_gpt:
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
    jmp mbr_boot_partition

.Lpart_no_good:
    addb $ 1,dap_lba(%si)
    cmpb $ 34,dap_lba(%si)
    jb .Lanother_gpt

//.space 220

mbr_disk_io_with_lba:
    movw %ax,mbr_dap+dap_lba
    movw %cx,mbr_dap+dap_lba+2
    // fall through
mbr_disk_io:
    movw $ mbr_dap,%si
    movw $ 0x4200,%ax
    int $ 0x13
    jc mbr_disk_error
    ret

mbr_disk_error:
    movw $ 'd'+0xF00,%ax
    jmp mbr_fail_message

mbr_bad_bootsector:
    movw $ 'b'+0xF00,%ax
    jmp mbr_fail_message

mbr_no_active_partition:
    movw $ 'p'+0xF00,%ax

mbr_fail_message:
    // Put one character and attribute in text mode video memory
    // at the top left corner of the screen
    pushw $ 0xb800
    popw %ds
    movw %ax,0

mbr_give_up:
    int $ 0x18
    jmp mbr_give_up


// ----------------------------------------------------------------------
// Data
.section .mbrdata, "aw", @progbits
mbr_init_stack:
    .hword 0x1000
    .hword 0x0000

mbr_dap:
    .byte dap_length
    .byte 0
    .hword 1
    .hword 0x7c00
    .hword 0x0000
    .quad 0

mbr_ptbl_ent_ptr:
    .hword 0

// ----------------------------------------------------------------------
.section .parttab

.macro partition_table_entry active,lba_start,lba_sectors,type
    .if \active != 0
        .byte 0x80
    .else
        .byte 0x00
    .endif

    .byte 0
    .hword 0

    .byte \type

    .byte 0
    .hword 0

    .int \lba_start
    .int \lba_sectors
.endm

.macro partition_table_entry_unused
    .byte 0
    .byte 0
    .hword 0
    .byte 0
    .byte 0
    .hword 0
    .int 0
    .int 0
.endm

mbr_partition_table:
    // partition_table_entry 1,128,(1 << (24-9)),0x0C
    partition_table_entry_unused
    partition_table_entry_unused
    partition_table_entry_unused
    partition_table_entry_unused

    .byte 0x55
    .byte 0xAA
