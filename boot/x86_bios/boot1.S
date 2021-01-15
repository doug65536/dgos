#include "ptbl.h"
#include "dap.h"

.code16

// MBR calls this code with:
//  DS:SI -> partition table entry
//  DL = boot drive

// This does just enough to relocate itself to its link address at 0x0C00,
// load 124KB of code from sector partition_start+2 to 0x1000
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

// Linked and relocated to 0x0C00 from 0x7c00
#define RELOC_OFS   0x0C00
#define LOAD_OFS    0x7C00
#define UNRELOCATED(n) (n+(LOAD_OFS-RELOC_OFS))

struct_begin chinf
struct_field 2 chinf_size
struct_field 2 chinf_seg
struct_field 4 chinf_ofs
struct_end chinf

.section .head, "ax", @progbits
// The linker script puts this at the beginning of the image

.global boot1_entry
boot1_entry:
    // Only 3 bytes available here, use shortest possible jump (2 bytes)
    // This type of jmp is position independent
    jmp boot1_bootstrap_code

// The linker script puts this after the BPB and stuff (at 0x5A)
.section .entry, "ax", @progbits
boot1_bootstrap_code:
    // Load cs register ASAP
    ljmpw $ 0,$ UNRELOCATED(.Lload_cs)

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
    movw %bx,UNRELOCATED(boot1_part_st)
    movw %cx,UNRELOCATED(boot1_part_st+2)
    movw %di,UNRELOCATED(boot1_part_en)
    movw %bp,UNRELOCATED(boot1_part_en+2)
    movb %dl,UNRELOCATED(boot1_boot_drive)

    // Set start sector for stage 2
    xor %ax,%ax
    addw $ 2,%bx
    movw %bx,UNRELOCATED(boot1_sector_lba)
    adcw %ax,%cx
    movw %cx,UNRELOCATED(boot1_sector_lba+2)
    movw %bx,UNRELOCATED(boot1_dap+dap_lba)
    movw %cx,UNRELOCATED(boot1_dap+dap_lba+2)
    movw %ax,UNRELOCATED(boot1_dap+dap_lba+4)
    movw %ax,UNRELOCATED(boot1_dap+dap_lba+6)

    // Relocate 512 bytes
    mov $ UNRELOCATED(boot1_entry),%si
    mov $ boot1_entry,%di
    mov $ ___boot1_image_end,%cx
    sub %di,%cx
    shr $ 1,%cx
    cld
    rep movsw

    // Relocation done, no more weird unrelocated offsets

    ljmpw $ 0,$ boot1_relocated_entry
boot1_relocated_entry:

    // Get drive parameters size (for sector size)
    movw $ 0x4800,%ax
    movw $ boot1_ddp,%si
    movw $ ddp_length,(%si)
    movb boot1_boot_drive,%dl
    int $ 0x13
    jc boot1_fail

boot1_chunk_loop:
    movw boot1_chunk_index,%si
    incw boot1_chunk_index
    shlw $ 3,%si

    // Fetch chunk file offset, divide by sector size
    movw boot1_chunks+chinf_ofs(%si),%ax
    movw boot1_chunks+chinf_ofs+2(%si),%dx
    divw boot1_ddp+ddp_sector_size
    addw boot1_sector_lba,%ax
    adcw boot1_sector_lba+2,%dx
    movw %ax,boot1_dap+dap_lba
    movw %dx,boot1_dap+dap_lba+2

    // Fetch chunk destination and update destination address
    movw boot1_chunks+chinf_seg(%si),%ax
    movw $ 0,boot1_dap+dap_address
    movw %ax,boot1_dap+dap_address+2

    // Fetch chunk size for sector size div
    movw boot1_chunks+chinf_size(%si),%ax

    // Calculate stage2 sector count, portion before 64KB
    // Load 0x1000-0x10000 region
    //movw $ 0xF000,%ax
    xor %dx,%dx
    divw boot1_ddp+ddp_sector_size
    // Update sector count that isn't overwritten
    mov %ax,boot1_sector_count
    // Update sector count boot1_dap parameter that is overwritten
    mov %ax,boot1_dap+dap_block_count

    // Load stage2
boot1_more_sectors:
    movw $ 0x4200,%ax
    movw $ boot1_dap,%si
    movb boot1_boot_drive,%dl
    int $ 0x13
    jnc boot1_next_chunk

    // It may be an error, or just a partial success. Check.

    // Read number of blocks actally transferred
    movw boot1_dap+dap_block_count,%ax

    // If no blocks transferred, it really failed
    test %ax,%ax
    jz boot1_fail

    // It was a partial success...

    // Adjust (remaining) sector count
    movw boot1_sector_count,%si
    subw %ax,%si
    movw %si,boot1_sector_count
    movw %si,boot1_dap+dap_block_count

    // Multiply the number of sectors transferred by the sector size
    mulw boot1_ddp+ddp_sector_size

    // Advance the address that many bytes
    // (by advancing the segment 1/16th as much)
    // Makes sense because the smallest sectors are much larger than 16 bytes
    shrw $ 4,%ax
    addw %ax,boot1_dap+dap_address+2
    test %si,%si
    jnz boot1_more_sectors

boot1_next_chunk:
    cmpw $ 3,boot1_chunk_index
    jb boot1_chunk_loop

    xor %si,%si
    mov %si,%ds
    mov %si,%es

    // Setup register parameters for stage 1
    // es:si -> partition table entry
    // dl = boot drive
    push %cs
    pop %es
    mov $ boot1_ptbl_ent,%si

    ljmpw $ 0,$ 0x1000

boot1_fail:
    int $ 0x18

.section .data

// dap structure for int 0x13, AX=0x4200. see dap.h "dap" for layout
boot1_dap:
// sizeof_dap
    .byte 16
// reserved
    .byte 0
//dap_sector_count:
    .hword 0
//dap_load_addr:
    .hword 0x1000
    .hword 0x0000
//dap_sector_lba:
    .quad 0

boot1_ptbl_ent:
    .byte 1
    .skip 7,0

boot1_part_st: .int 0
boot1_part_en: .int 0

boot1_sector_count: .hword 0

boot1_chunk_index: .word 0

boot1_chunks:
    // Load 0xF000 (60KB) bytes to 0x1000 (seg 0x100), file offset 0
    .hword 0xF000
    .hword 0x100
    .int 0
    // Load 0x8000 (32KB) bytes to 0x10000 (seg 0x1000), file offset 0xF000
    .hword 0x8000
    .hword 0x1000
    .int 0xF000
    // Load 0x8000 (32KB) bytes to 0x18000 (seg 0x1800), file offset 0x17000
    .hword 0x8000
    .hword 0x1800
    .int 0x17000
boot1_chunks_end:

boot1_boot_drive: .byte 0

.byte 0xEE

.section .uninit, "w", @nobits

boot1_ddp:
    .skip ddp_length,0

boot1_sector_lba:
    .skip 8
