#include "ptbl.h"
#include "dap.h"

.code16

// MBR calls this code with:
//  DS:SI -> partition table entry
//  DL = boot drive

// This does just enough to relocate itself to its link address at 0xF000,
// load 56KB of code from sector partition_start+2 to 0x1000
// set ds:bx and dl parameters, and jump to 0x7C00
// Invokes interrupt 0x18 on error
//
// The purpose of decoupling this from the bootloader is to
// make the bootloader layout independent of the sector size
// and to allow for the FS information sector to be an unknown size
// (which is the size of one sector)

// Layout
//
// +--------+------+--------------------+
// | offset | size | description        |
// +--------+------+--------------------+
// |     0  |   11 | jmp                |
// +--------+------+--------------------+
// |   0xB  |   79 | BPB                |
// +--------+------+--------------------+
// |  0x5A  |  404 | code               |
// +--------+------+--------------------+
// + 0x1FE  |    2 | signature (0x55AA) |
// +--------+------+--------------------+

#define UNRELOCATED(n) (n)-0xF000+0x7C00

.section .head
.global entry
entry:
    ljmp $0,$ UNRELOCATED(past_bpb)

.section .code
past_bpb:
//    xor %ax,%ax
//    0:
//    test %ax,%ax
//    jz 0b
    
    // Load the relevant portion of the partition table entry
    // into registers before writing any memory
    // (The CHS parts are discarded)
    movw ptbl_ent_stsec(%si),%bx
    movw ptbl_ent_stsec+2(%si),%cx
    movw ptbl_ent_numsec(%si),%di
    movw ptbl_ent_numsec+2(%si),%bp
    
    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %bx,UNRELOCATED(part_st)
    movw %cx,UNRELOCATED(part_st+2)
    movw %di,UNRELOCATED(part_en)
    movw %bp,UNRELOCATED(part_en+2)
    movb %dl,UNRELOCATED(boot_drive)
    
    // Set start sector for stage 2
    addw $2,%bx
    adcw $0,%cx
    movw %bx,UNRELOCATED(sector_lba)
    movw %cx,UNRELOCATED(sector_lba+2)
    
    // Relocate 512 bytes
    mov $ entry,%di
    mov $ 0x7C00,%si
    mov $ 256,%cx
    cld
    rep movsw
    
    ljmp $0,$ reloc_entry
reloc_entry:

    // Get drive parameters size (for sector size)
    mov $0x4800,%ax
    mov $ddp,%si
    movw $ ddp_length,ddp_struct_size(%si)
    int $0x13
    jc fail
    
    // Calculate stage2 sector count
    // 64KB (of reserved sectors) 
    // minus 4KB (0x1000 base)
    // minus 4KB (reserved for this code) = 57344 bytes.
    movw $ 0x10000 - 4096 - 4096,%ax
    xorw %dx,%dx
    divw ddp+ddp_sector_size
    mov %ax,sector_count
    
    // Load stage2
    movw $0x4200,%ax
    movw $dap,%si
    movb boot_drive,%dl
    int $0x13
    jc fail
    
    // Setup register parameters for stage 1
    // ds:si -> partition table entry
    // dl = boot drive
    mov $ ptbl_ent,%si    
    
    ljmpw *load_addr
    
fail:
    int $0x18

// dap structure for int 0x13, AX=0x4200. see dap.h "dap" for layout
dap:
    .byte 16
    .byte 0
sector_count:
    .word 0
load_addr:
    .word 0x1000
    .word 0x0000
sector_lba:
    .quad 0

ptbl_ent: 
    .byte 1
    .space 7,0
part_st:
    .space 4
part_en:
    .space 4


boot_drive: .space 1
ddp: .space ddp_length

.byte 0xEE

.section .uninit

