// This is the entry point code for BIOS CD boot

#include "dap.h"

.code16

// The BIOS is told by the el-torito PVD to only load the first 28KB
// from 0x1000-0x8000
// We need to load the remaining code at 0x8000

#define ALREADY_LOADED_START 0x1000

// End address of data already loaded by firmware
#define ALREADY_LOADED_END 0x8000

#define ALREADY_LOADED_SZ (ALREADY_LOADED_END - ALREADY_LOADED_START)

// Number of LBAs already loaded by firmware
#define ALREADY_LOADED_SECTOR_COUNT (ALREADY_LOADED_SZ >> 11)

// Apply an adjustment to get its offset from the relocated base
#define ZEROBASED(n) ((n) - ALREADY_LOADED_START)

// This is linked at the beginning of the binary
.section .head, "ax", @progbits
.global bootiso_entry
bootiso_entry:
    // Load cs with zero and use 0x1000 offset
    // Just in case the bios actually jumped to cs=0x100,ip=0
    ljmpw $ 0,$ entry_start

.balign 8
.global bootinfo_primary_volume_desc
bootinfo_primary_volume_desc:
    .space 4

.global bootinfo_file_location
bootinfo_file_location:
    .space 4

.global bootinfo_file_length
bootinfo_file_length:
    .space 4

.global bootinfo_checksum
bootinfo_checksum:
    .space 4

.global bootinfo_reserved
bootinfo_reserved:
    .space 10*4

entry_start:
    .cfi_startproc
    .cfi_def_cfa esp,0
    .cfi_undefined eip
    cli
    cld

    // Initialize segment registers
    xor %eax,%eax
    mov %ax,%ds
    mov %ax,%es

    // Store boot drive passed in from BIOS
    movb %dl,boot_drive

    // Initialize relocated stack
    mov $ 0x2000,%eax
    mov %ax,%ss
    xor %esp,%esp

    // Load the whole cs:eip
    ljmpw $ 0,$ relocated_bootiso_entry

    .cfi_endproc

// This is linked after head
.section .lowtext, "ax", @progbits
.global relocated_bootiso_entry
relocated_bootiso_entry:
    .cfi_startproc
    .cfi_def_cfa esp,0
    .cfi_undefined eip
    callw clear_bss

.Lretry_io:
    // Load the rest of bootiso-bin

    // Load pointer to bootiso_dap
    // so we don't repeat the offset in memory accesses
    mov $ bootiso_dap,%esi

    // Initialize constant fields
    movw $ dap_length,dap_sizeof_packet(%si)

    // Translate destination address to a far pointer with minimal offset
    movl $ ALREADY_LOADED_END,%edx
    movl %edx,%eax
    andl $ 0xF,%eax
    shrl $ 4,%edx
    movw %ax,dap_address(%si)
    movw %dx,dap_address+2(%si)

    // Calculate the LBA
    movl bootinfo_file_location,%eax
    xorl %edx,%edx
    addl $ ALREADY_LOADED_SECTOR_COUNT,%eax
    adcl $ 0,%edx
    movl %eax,dap_lba(%si)
    movl %edx,dap_lba+4(%si)

    // Calculate how many 2KB sectors we need to load and set block count
    // The absolute maximum bootloader size is 124KB, so this is guaranteed
    // to be less than 62
    movl $ ___initialized_data_en,%ecx
    subl $ ALREADY_LOADED_END,%ecx
    shrl $ 11,%ecx

    // Keep count on stack
    push %ecx

.Lread_more:
    // expects (%esp) is in ecx already here

    // Over 32KB?
    cmpl $ 0x8000 >> 11,%ecx
    jb .Lno_block_clamp

    // Clamp to 32KB to avoid errors crossing 64KB boundaries
    movl $ 0x8000 >> 11,%ecx

.Lno_block_clamp:
    movw %cx,dap_block_count(%si)

    // Read the sectors
    movl $ 0x4200,%eax
    movzbl boot_drive,%edx
    int $ 0x13
    jnc .Lread_succeeded

    // Failed

    // Read back how many blocks actually transferred
    movzwl dap_block_count(%si),%ecx

    testl %ecx,%ecx
    jz load_failed

.Lread_succeeded:
    // Advance LBA
    addl %ecx,dap_lba(%si)
    adcl $ 0,dap_lba+4(%si)

    // Advance destination segment
    // (2KB sectors -> 16 byte real mode paragraphs)
    movl %ecx,%eax
    shll $ 11 - 4,%eax
    addw %ax,dap_address+2(%si)

    // Reduce remaining block count
    mov (%esp),%eax
    subl %ecx,%eax
    mov %eax,(%esp)
    mov %eax,%ecx

    jnz .Lread_more

load_done:	// utility label for placing debugger breakpoint

    addl $ 4,%esp

    callw detect_ancient_cpu

    fninit

    movl bootinfo_primary_volume_desc,%eax
    xorl %edx,%edx
    movl $ 0xb800,%ecx
    movw %cx,%gs
    movl $ iso9660_boot_partition,%ecx
    gs movw $ 0x700 | ':',80 * 25 * 2 - 4
    gs movw $ 0x700 | ')',80 * 25 * 2 - 2
    movl $ 0f,%ebx
    jmp boot

load_failed:
    movl $ 0xb800,%ecx
    movw %cx,%gs
    gs movw $ 0x700 | ':',80 * 25 * 2 - 4
    gs movw $ 0x700 | '(',80 * 25 * 2 - 2

0:  hlt
    jmp 0b

    .cfi_endproc

.balign 64
.global boot_drive
boot_drive:
    .byte 0

.section .lowbss, "aw", @nobits
bootiso_dap:
    .space dap_length

