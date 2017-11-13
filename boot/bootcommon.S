#include "gdt.h"

.macro gdt_seg priv, exec, rw, gran, is32, is64, limit_hi
    // limit 15:0
    .word 0xFFFF
    // base 15:0
    .word 0
    // base 23:16
    .byte 0
    // present, privilege, 1, executable, rw, 0
    .byte (1 << 7) | (\priv << 5) | (1 << 4) | (\exec << 3) | (\rw << 1)
    // granularity, is32, is64, limit 23:16
    .byte (\gran << 7) | (\is32 << 6) | (\is64 << 5) | \limit_hi
    // base 31:24
    .byte 0
.endm

.macro gdt_seg_code priv, gran, is32, is64, limit_hi
    gdt_seg \priv, 1, 1, \gran, \is32, \is64, \limit_hi
.endm

.macro gdt_seg_data priv, gran, is32, limit_hi
    gdt_seg \priv, 0, 1, \gran, \is32, 0, \limit_hi
.endm

.macro gdt_emit_desc sel
    .if \sel == GDT_SEL_KERNEL_CODE64
        gdt_seg_code 0,1,0,1,0xF

    .elseif \sel == GDT_SEL_KERNEL_DATA
        gdt_seg_data 0,1,1,0xF

    .elseif \sel == GDT_SEL_KERNEL_CODE32
        gdt_seg_code 0,1,1,0,0xF

    .elseif \sel == GDT_SEL_KERNEL_DATA32
        gdt_seg_data 0,1,1,0xF

    .elseif \sel == GDT_SEL_KERNEL_CODE16
        gdt_seg_code 0,0,0,0,0x0

    .elseif \sel == GDT_SEL_KERNEL_DATA16
        gdt_seg_data 0,0,0,0x0

    .elseif \sel == GDT_SEL_USER_CODE32
        gdt_seg_code 3,1,1,0,0xF

    .elseif \sel == GDT_SEL_USER_DATA
        gdt_seg_data 3,1,1,0xF

    .elseif \sel == GDT_SEL_USER_CODE64
        gdt_seg_code 3,1,0,1,0xF

    .else
        .error "unknown selector"

    .endif
.endm

.macro gdt_emit_desc_after sel, after
    .if \sel != \after + 8
        .error "GDT order not matching constants"
    .endif

    gdt_emit_desc \sel
.endm

.global gdt
.global gdtr

.section .data
gdt:
    .word 0
gdtr:
    .word 8*11
    .int gdt

    gdt_emit_desc_after GDT_SEL_KERNEL_CODE64, 0
    gdt_emit_desc_after GDT_SEL_KERNEL_DATA, GDT_SEL_KERNEL_CODE64
    gdt_emit_desc_after GDT_SEL_KERNEL_CODE32, GDT_SEL_KERNEL_DATA
    gdt_emit_desc_after GDT_SEL_KERNEL_DATA32, GDT_SEL_KERNEL_CODE32
    gdt_emit_desc_after GDT_SEL_KERNEL_CODE16, GDT_SEL_KERNEL_DATA32
    gdt_emit_desc_after GDT_SEL_KERNEL_DATA16, GDT_SEL_KERNEL_CODE16
    gdt_emit_desc_after GDT_SEL_USER_CODE32, GDT_SEL_KERNEL_DATA16
    gdt_emit_desc_after GDT_SEL_USER_DATA, GDT_SEL_USER_CODE32
    gdt_emit_desc_after GDT_SEL_USER_CODE64, GDT_SEL_USER_DATA

.section .text

.code16

// Inputs:
//  eax: parameter to partition boot call
//  edx: pointer to partition boot function

.global boot
boot:
    mov %eax,%ebx
    mov %edx,%esi

    call idt_init
    call check_a20

    cli
    lgdt gdtr

    mov %cr0,%eax
    bts $0,%eax
    mov %eax,%cr0

    ljmp $GDT_SEL_KERNEL_CODE32,$0f
0:
.code32
    mov $GDT_SEL_KERNEL_DATA,%edx
    mov %edx,%ds
    mov %edx,%es
    mov %edx,%fs
    mov %edx,%gs
    mov %edx,%ss
    and $-16 & 0xFFFF,%esp

    call cpu_a20_enterpm

    call call_constructors

    mov %esp,%edi
    mov %ebx,%eax
    call *%esi
    mov %edi,%esp

    call cpu_a20_exitpm

    // Jump to 16 bit protected mode and load segments
    ljmp $GDT_SEL_KERNEL_CODE16,$0f
.code16
0:
    movw $GDT_SEL_KERNEL_DATA16,%dx
    mov %dx,%ds
    mov %dx,%es
    mov %dx,%fs
    mov %dx,%gs
    mov %dx,%ss

    // Turn off protected mode
    mov %cr0,%eax
    btr $0,%eax
    mov %eax,%cr0

    // Jump to real mode
    ljmp $0,$0f
0:

    xor %edx,%edx
    mov %edx,%ds
    mov %edx,%es
    mov %edx,%fs
    mov %edx,%gs
    mov %edx,%ss

    ret

.code16

.global idt_init
idt_init:
    mov $idt,%edx
    mov $0,%ecx
0:
    movw isr_table(,%ecx,4),%ax
    movw %ax,(%edx,%ecx,8)
    movw $8,2(%edx,%ecx,8)
    movw $0,4(%edx,%ecx,8)
    movw $0x8e,5(%edx,%ecx,8)
    movw $0,6(%edx,%ecx,8)
    incl %ecx
    cmpl $32,%ecx
    jb 0b

    movw $32 * 8 - 1,idtr_64
    movw $idt,idtr_64 + 2

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

.code32

call_constructors:
    push %ebx
    movl $__ctors_start,%ebx
0:
    cmpl $__ctors_end,%ebx
    jae 0f
    movl (%ebx),%eax
    test %eax,%eax
    jz 1f
    cmpl $entry,%eax
    jb 1f
    call *%eax
1:
    addl $4,%ebx
    jmp 0b
0:
    pop %ebx
    ret
