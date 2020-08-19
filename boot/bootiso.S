#include "dap.h"

.code16

// The BIOS is told by the el-torito PVD to only load the first 4KB to 0x1000
// We need to load the remaining code at 0x2000

// Apply an adjustment to get its offset from the relocated base
#define ZEROBASED(n) n - 0x1000

// This is linked at the beginning of the binary
.section .head, "wx", @progbits
.global entry
entry:
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
    cli
    cld

    // Initialize segment registers
    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es

    // Store boot drive passed in from BIOS
    movb %dl,boot_drive

    // Initialize relocated stack
    mov $ 0x2000,%ax
    mov %ax,%ss
    xor %sp,%sp

    ljmpw $ 0,$ relocated_bootiso_entry

// This is linked after head
.section .early, "wx", @progbits
relocated_bootiso_entry:
    callw clear_bss

.Lretry_io:
    // Load the rest of bootiso-bin

    // Load pointer to dap so we don't repeat the offset in memory accesses
    movw $ dap,%si

    // Initialize constant fields
    movb $ dap_length,dap_sizeof_packet(%si)
    movb $ 0,dap_reserved(%si)

    // Translate destination address to a far pointer with minimal offset
    movl $ ___text_st + 4096,%edx
    movw %dx,%ax
    andw $ 0xF,%ax
    shrl $ 4,%edx
    movw %ax,dap_address(%si)
    movw %dx,dap_address+2(%si)

    // Calculate the LBA
    movl bootinfo_file_location,%eax
    add $ 2,%eax
    xorl %edx,%edx
    movl %eax,dap_lba(%si)
    movl %edx,dap_lba+4(%si)

    // Calculate how many 2KB sectors we need to load and set block count
    movl $ ___initialized_data_en,%ecx
    subl $ ___text_st + 4096,%ecx
    shrl $ 11,%ecx

.Lread_more:
    movw %cx,dap_block_count(%si)

    // Read the sectors
    movw $ 0x4200,%ax
    movb boot_drive,%dl
    int $ 0x13
    jnc load_done

    // Failed

    // Read back how many blocks actually transferred
    movzwl dap_block_count(%si),%eax

    // Advance LBA
    addl %eax,dap_lba(%si)
    adcl $ 0,dap_lba+4(%si)

    // Advance destination segment
    // (2KB sectors -> 16 byte real mode paragraphs)
    shll $ 11 - 4,%eax
    addw %ax,dap_address+2(%si)

    // Reduce remaining block count
    sub %eax,%ecx

    jnz .Lread_more

load_done:	// utility label for placing debugger breakpoint

    callw detect_ancient_cpu

    fninit

    movl bootinfo_primary_volume_desc,%eax
    xorl %edx,%edx
    mov $ 0xb800,%ecx
    mov %cx,%gs
    movl $ iso9660_boot_partition,%ecx
    gs movb $ '!',80*25*2-2
    movl $ 0f,%ebx
    jmp boot
0:  hlt
    jmp 0b

.balign 64
.global boot_drive
boot_drive:
    .byte 0

.section .bss, "aw", @nobits
dap:
    .space dap_length

