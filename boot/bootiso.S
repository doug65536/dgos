#include "dap.h"

.code16

// Note that before relocate is executed, all addresses are off by 0x6C00
// because the code is linked at 0x1000 but loaded at 0x7C00
//
// The BIOS is told by the el-torito PVD to only load the first 4KB
// before jumping to entry at 0x7C00
// We need to relocate it to 0x1000 then load the remaining code at 0x2000

// Apply an adjustment to a symbol to get its address before relocation
#define UNRELOCATED(n) n + 0x7C00 - 0x1000

// Apply an adjustment to get its offset from the relocated base
#define ZEROBASED(n) n - 0x1000

// This is linked at the beginning of the binary
.section .head, "wx", @progbits
.global entry
entry:
    // Jump to the unrelocated "entry_start"
    ljmpw $0,$ UNRELOCATED(entry_start)

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

    // Store boot drive passed in from BIOS to unrelocated "boot_drive"
    movb %dl,UNRELOCATED(boot_drive)

    // Initialize relocated stack
    movw %ax,%ss
    movl $ ___initial_stack,%esp

    // Don't touch the stack until "relocate" is done!

    // Calculate unrelocated linear address of relocate_farptr -> bx
    movw $ UNRELOCATED(relocate_farptr),%bx

    //
    // Copy the code between "relocate" and "relocate_end"
    // to unrelocated ___end
    //

    // Calculate unrelocated seg:off of "___end" symbol into ax:edi
    movl $ UNRELOCATED(___end),%edi
    movl %edi,%eax
    andw $0xF,%di
    shrl $4,%eax
    movw %ax,%es

    // Store the far pointer to the relocated "relocate" to relocate_farptr
    movw %di,(%bx)
    movw %ax,2(%bx)

    // Destination segment is ax
    movw %ax,%es

    // es:di now points to unrelocated ___end

    // Copy code
    movw $ UNRELOCATED(relocate),%si
    movw $ ZEROBASED(relocate_end),%cx
    subw $ ZEROBASED(relocate),%cx
    cld
    rep movsb

    // Far jump into copied relocate code
    // Note that we went to all this trouble because it ends up above 64KB line
    // and we move it to utterly guarantee relocate won't overwrite itself in
    // the middle of its execution
    ljmpw *(%bx)

.balign 4
relocate_farptr:
    .space 4

// Relocate the first 4KB (0x7C00-0x8BFF) to (0x1000-0x1FFF)
relocate:
    xorw %cx,%cx
    movw %cx,%es
    movw $4096,%cx
    movw $ ___text_st,%di
    shrw $2,%cx
    movw $ UNRELOCATED(___text_st),%si
    cld
    rep movsl

    // Jump into relocate. All the craziness is now over, symbols are correct
    // and everything is < 64KB line
    ljmpw $0,$reloc_entry
relocate_end:

// This is linked after head
.section .early, "wx", @progbits
reloc_entry:
    callw clear_bss

.Lretry_io:
    // Load the rest of bootiso-bin
    movw $ dap,%si
    movb $ dap_length,dap_sizeof_packet(%si)
    movb $ 0,dap_reserved(%si)
    movl $ ___initialized_data_en,%ecx
    subl $ ___text_st + 4096,%ecx
    shrl $ 11,%ecx
    movw %cx,dap_block_count(%si)
    movl $ ___text_st + 4096,%edx
    movw %dx,%ax
    andw $0xF,%ax
    shrl $ 4,%edx
    movw %ax,dap_address(%si)
    movw %dx,dap_address+2(%si)
    movl bootinfo_file_location,%eax
    add $2,%eax
    xorl %edx,%edx
    movl %eax,dap_lba(%si)
    movl %edx,dap_lba+4(%si)

    movw $0x4200,%ax
    movb boot_drive,%dl
    int $0x13
    jnc load_done

load_done:	// utility label for placing debugger breakpoint

    callw detect_ancient_cpu

    fninit

    movl bootinfo_primary_volume_desc,%eax
    xorl %edx,%edx
    movl $iso9660_boot_partition,%ecx
    gs movb $'!',80*25*2-2
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

