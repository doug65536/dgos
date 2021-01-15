// This is the entry point code for BIOS FAT boot

#include "dap.h"
#include "ptbl.h"

.code16

// Note that this code goes out of its way to avoid using any
// instructions that will not work on an 8088. Let's assume it's
// possible for completeness!

// stage1 calls this code with:
//  ES:SI -> start LBA of partition
//  DL = boot drive

.section .lowtext, "ax", @progbits
.global bootfat_entry
bootfat_entry:
    movb %dl,boot_drive
    movw %si,part_tbl_ptr
    movw %es,part_tbl_ptr+2

    movw $ init_ret,%bx
    jmp early_init
init_ret:

    // If we make it here, the cpu is at least 32 bit with FPU

    // Load partition start LBA into edx:eax
    lesw part_tbl_ptr,%si
    movl %es:ptbl_ent_stsec(%si),%eax
    xorl %edx,%edx
    movw %dx,%es

    movl $ fat32_boot_partition,%ecx
    movl $ 0f,%ebx
    jmp boot
0:  hlt
    jmp 0b

.global boot_drive
boot_drive:
    .byte 0

.balign 4
.global part_tbl_ptr
part_tbl_ptr:
    .hword 0
    .hword 0
