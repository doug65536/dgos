#pragma once

.macro insn_fixup
.Linsn_fixup_\@\():
.pushsection .rodata.fixup.insn
    .quad .Linsn_fixup_\@
.popsection
.endm

.macro vzeroall_insn
    insn_fixup
    vzeroall
    // 2 byte nop to expand to 5 byte (replaceable with a call)
    .byte 0x66, 0x90
.endm

.macro swapgs_boundary
.Lswapgs_boundary_\@\():
    .pushsection .rodata.fixup.swapgs
    .quad .Lswapgs_boundary_\@
.popsection
.endm
