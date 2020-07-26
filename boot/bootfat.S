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

.section .early, "wx", @progbits
.global entry
entry:
    mov %dl,boot_drive

    mov $ .Linit_ret,%bx
    jmp early_init
.Linit_ret:

    // If we make it here, the cpu is at least 32 bit with FPU

    // Load partition start LBA into edx:eax
    movl ptbl_ent_stsec(%si),%eax
    xor %edx,%edx

    movl $ fat32_boot_partition,%ecx
    movl $ 0f,%ebx
    jmp boot
0:  hlt
    jmp 0b

.global boot_drive
boot_drive:
    .byte 0
