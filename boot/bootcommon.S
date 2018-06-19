#include "gdt_sel.h"

.section .text

.code16

// Inputs:
//  edx:eax: parameter to partition boot call
//  ecx: pointer to partition boot function

.global boot
boot:
    push %ebp
    mov %esp,%ebp
    push %edi
    push %esi
    push %ebx

    // ebx:esi holds boot LBA
    // edi holds boot function
    mov %eax,%ebx
    mov %edx,%esi
    mov %ecx,%edi

    call idt_init
    call check_a20

    cli
    movl $gdt,2+gdtr
    lgdt gdtr

    mov %cr0,%eax
    bts $0,%eax
    mov %eax,%cr0

    movw $GDT_SEL_PM_DATA32,%dx
    ljmp $GDT_SEL_PM_CODE32,$pmode_entry
pmode_entry:
.code32
    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs
    movw %dx,%ss
    mov $___initial_stack,%esp

    xor %eax,%eax
    lldt %ax

    // Clean eflags
    pushl $2
    popfl

    call cpu_a20_enterpm

    call ctors_invoke

    mov %ebx,%eax
    mov %esi,%edx
    call *%edi

    call cpu_a20_exitpm

    // Jump to 16 bit protected mode and load segments
    ljmp $GDT_SEL_PM_CODE16,$0f
.code16
0:
    movw $GDT_SEL_PM_DATA16,%dx
    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs
    movw %dx,%ss

    // Turn off protected mode
    mov %cr0,%eax
    btr $0,%eax
    mov %eax,%cr0

    // Jump to real mode
    ljmp $0,$0f
0:

    xor %edx,%edx
    mov %dx,%ds
    mov %dx,%es
    mov %dx,%fs
    mov %dx,%gs
    mov %dx,%ss

    pop %ebx
    pop %esi
    pop %edi
    pop %ebp

    ret

.code16

// Returns if the CPU is at least an 80486 with CPUID
// does not return if CPU is too old to be usable
// clobbers eax, ecx, edx
.global detect_ancient_cpu
detect_ancient_cpu:
    // An 8086 has flags 15:12 permanently set
    mov $0x0FFF,%cx
    xor %dx,%dx
    call change_flags
    test $0xF000,%ax
    jnz detected_8086

    // An 80286 has flags 15:12 permanently clear in real mode
    or $-1,%cx
    mov $0xF000,%dx
    call change_flags
    test $0xF000,%ax
    jz detected_80286

    // An 80386 has eflags 18 (AC) permanently clear
    or $-1,%ecx
    mov $ 1 << 18,%edx
    call change_eflags
    test $ 1 << 18,%eax
    jz detected_80386

    //
    // An early 80486 has eflags 21 (ID) stuck

    // Isolate bit 21 and save it
    and $ 1 << 21,%eax
    push %eax

    // Clear nothing, set nothing
    or $-1,%ecx
    xor %edx,%edx

    // Clear it if it was set
    xor %eax,%ecx

    // Set it if it was clear
    xor $ 1 << 21,%eax
    or %eax,%edx

    call change_eflags
    pop %ecx
    cmp %ecx,%eax
    jz detected_old_486

    // CPUID instruction will work
    ret

str_prefix:	.asciz "An "
str_8086:	.asciz "8086"
str_80286:	.asciz "80286"
str_80386:	.asciz "80386"
str_80486:	.asciz "early 80486"
str_suffix:	.asciz " cannot run this operating system"

detected_8086:
    mov $str_8086,%bx
    jmp detection_failed

detected_80286:
    mov $str_80286,%bx
    jmp detection_failed

detected_80386:
    mov $str_80386,%bx
    jmp detection_failed

detected_old_486:
    mov $str_80486,%bx

// ...fall through
detection_failed:
    mov $str_prefix,%di
    call fail_str
    mov %bx,%di
    call fail_str
    mov $str_suffix,%di
    call fail_str
    cli
0:
    hlt
    jmp 0b

// Clobbers si, di
fail_str:
    mov $0xb800,%ax
    mov %ax,%es
    mov $0x0800,%ax
0:
    lodsb
    test %al,%al
    jz 0f
    stosw
    jmp 0b
0:
    xor %ax,%ax
    mov %ax,%es
    ret

// Read flags,
// bitwise and flags with %cx,
// bitwise or flags with %dx
// put flags into %ax
change_flags:
    pushf
    popw %ax
    andw %cx,%ax
    orw %dx,%ax
// ... fall through
// Write ax to the flags register then update ax with updated flags
update_flags:
    pushw %ax
    popfw
    pushfw
    popw %ax
    ret

change_eflags:
    pushfl
    popl %eax
    andl %ecx,%eax
    orl %edx,%eax
// ... fall through
// Write eax to the eflags register then update eax with updated eflags
update_eflags:
    pushl %eax
    popfl
    pushfl
    popl %eax
    ret

.global idt_init
idt_init:
    mov $idt,%edx
    mov $0,%ecx

0:
    movw isr_table(,%ecx,4),%ax
    movw %ax,(%edx,%ecx,8)
    movw $GDT_SEL_KERNEL_CODE64,2(%edx,%ecx,8)
    movw $0,4(%edx,%ecx,8)
    movw $0x8e,5(%edx,%ecx,8)
    movw $0,6(%edx,%ecx,8)
    incl %ecx
    cmpl $32,%ecx
    jb 0b

    // Set limit
    movw $32 * 8 - 1,idtr_64 + 6
    movw $idt,idtr_64 + 8

    ret

.global check_a20
check_a20:
    mov $0xFFFF,%ax
    movw %ax,%fs

    // Attempt to read boot sector signature through wraparound
    movw %fs:entry + 0x1fe + 0x10,%ax

    // Read the boot sector signature without wraparound
    movw entry + 0x1fe,%cx
    cmp %ax,%cx
    je 0f

    // A20 is on
1:
    movw $0,%ax
    movw %ax,%fs
    movw $0,need_a20_toggle
    movw $1,%ax
    ret

0:
    // They were the same value! Change the boot sector signature

    notw entry + 0x1fe
    wbinvd

    // Attempt to read boot sector signature through wraparound again
    movw %fs:entry + 0x1fe + 0x10,%ax

    cmp %ax,%cx
    jne 1b

    // A20 is off
    movw $0,%ax
    movw %ax,%fs
    movw $1,need_a20_toggle
    movw $0,%ax
    ret

.code16
.section .early

.global clear_bss
clear_bss:
    pushw %di
    pushw %cx
    pushw %ax
    movw $ ___bss_start,%di
    movw $ ___bss_end,%cx
    subw %di,%cx
    xorb %al,%al
    cld
    rep
    stosb
    popw %ax
    popw %cx
    popw %di
    ret

.section .parttab
.space 16*4
.word 0xAA55

.global first_sector_end
first_sector_end:

.code32
.section .text
