#include "gdt_sel.h"

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

    .elseif \sel == GDT_SEL_PM_CODE32
        gdt_seg_code 0,1,1,0,0xF

    .elseif \sel == GDT_SEL_PM_DATA32
        gdt_seg_data 0,1,1,0xF

    .elseif \sel == GDT_SEL_PM_CODE16
        gdt_seg_code 0,0,0,0,0x0

    .elseif \sel == GDT_SEL_PM_DATA16
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

.section .data

.global gdt
.global gdtr

.align 8
gdt:
    .word 0
gdtr:
    .word 8*24
    .int gdt

    gdt_emit_desc GDT_SEL_PM_CODE16
    gdt_emit_desc GDT_SEL_PM_DATA16

    gdt_emit_desc GDT_SEL_PM_CODE32
    gdt_emit_desc GDT_SEL_PM_DATA32

    .quad 0
    .quad 0
    .quad 0

    gdt_emit_desc GDT_SEL_USER_CODE32
    gdt_emit_desc GDT_SEL_USER_DATA
    gdt_emit_desc GDT_SEL_USER_CODE64

    .quad 0

    gdt_emit_desc GDT_SEL_KERNEL_CODE64
    gdt_emit_desc GDT_SEL_KERNEL_DATA

    .quad 0
    .quad 0

    .quad 0
    .quad 0

    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
