#include "ptbl.h"
#include "dap.h"

.code16

// MBR calls this code with:
//  DS:SI -> partition table entry
//  DL = boot drive

// This does just enough to relocate itself to its link address at 0xF000,
// load ~56KB of code from sector partition_start+2 to 0x1000 less a sector
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

.section .head, "x", @progbits
// The linker script puts this at the beginning of the image

.global entry
entry:
    // Only 3 bytes available here, use shortest possible jump (2 bytes)
    // This type of jmp is position independent
    jmp bootstrap_code

// The linker script puts this after the BPB and stuff (at 0x5A)
.section .code, "x", @progbits
bootstrap_code:
    // Load cs register ASAP
    ljmp $ 0,$ UNRELOCATED(.Lload_cs)

.Lload_cs:
    cli

    // Load the relevant portion of the partition table entry
    // into registers before writing any memory
    // with whatever ds the MBR gave us
    // (The CHS parts are discarded)
    movw ptbl_ent_stsec(%si),%bx
    movw ptbl_ent_stsec+2(%si),%cx
    movw ptbl_ent_numsec(%si),%di
    movw ptbl_ent_numsec+2(%si),%bp

    // Initialize the data segment registers and stack
    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es

    // Entire 64KB from 0x20000 thru 0x2FFFF is the stack
    // (1st push will wrap it around to 0x1000:0xFFFE (0x1FFFE))
    // Tolerate the most stack hungry firmware
    // Be careful! Use of bp in a memory address calculation causes
    // the cpu to default to ss segment instead of ds
    movw $ 0x2000,%ax
    mov %ax,%ss
    xor %sp,%sp

    // Store the information loaded from pointer to partition table entry
    movw %bx,UNRELOCATED(part_st)
    movw %cx,UNRELOCATED(part_st+2)
    movw %di,UNRELOCATED(part_en)
    movw %bp,UNRELOCATED(part_en+2)
    movb %dl,UNRELOCATED(boot_drive)

    // Set start sector for stage 2
    addw $ 2,%bx
    adcw $ 0,%cx
    movw %bx,UNRELOCATED(sector_lba)
    movw %cx,UNRELOCATED(sector_lba+2)

    // Relocate 512 bytes
    mov $ entry,%di
    mov $ 0x7C00,%si
    mov $ 256,%cx
    cld
    rep movsw

    // Relocation done, no more weird unrelocated offsets

    ljmp $ 0,$ relocated_boot1_entry
relocated_boot1_entry:

    // Get drive parameters size (for sector size)
    mov $ 0x4800,%ax
    mov $ ddp,%si
    movb boot_drive,%dl
    int $ 0x13
    jc fail

    // Calculate stage2 sector count
    // Load 60KB
    movw $ 0xE000,%ax
    xorw %dx,%dx
    divw ddp+ddp_sector_size
    mov %ax,sector_count

    // Load stage2
more_sectors:
    movw $ 0x4200,%ax
    movw $ dap,%si
    movb boot_drive,%dl
    int $ 0x13
	jnc okay

	// It may be an error, or just a partial success. Check.

	// Read number of blocks actally transferred
	movw dap+dap_block_count,%ax

	// If no blocks transferred, it really failed
	test %ax,%ax
	jz fail

	// It was a partial success...

	// Adjust (remaining) sector count
	movw sector_count,%si
	subw %ax,%si
	movw %si,dap+dap_block_count
	movw %si,sector_count

	// Multiply the number of sectors transferred by the sector size
	mulw ddp+ddp_sector_size

	// Advance the address that many bytes
    // (by advancing the segment 1/16th as much)
    // Makes sense because the smallest sectors are much larger than 16 bytes
    shrw $ 4,%ax
	addw %ax,dap+dap_address+2
	test %si,%si
  	jnz more_sectors

okay:
    // Setup register parameters for stage 1
    // ds:si -> partition table entry
    // dl = boot drive
    mov $ ptbl_ent,%si

    ljmpw *load_addr

fail:
    int $ 0x18

.section .data

// dap structure for int 0x13, AX=0x4200. see dap.h "dap" for layout
dap:
    .byte 16
    .byte 0
sector_count:
    .hword 0
load_addr:
    .hword 0x1000
    .hword 0x0000
sector_lba:
    .quad 0

ptbl_ent:
    .byte 1
    .skip 7,0

part_st:
    .skip 4,0
part_en:
    .skip 4,0

boot_drive: .skip 1,0
ddp: .hword ddp_length
.skip (ddp_length - 2),0

.byte 0xEE

.section .uninit, "w", @nobits

