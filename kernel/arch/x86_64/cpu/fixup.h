#pragma once

.macro insn_fixup
.Linsn_fixup_\@\():
.pushsection .rodata.fixup.insn
    .quad .Linsn_fixup_\@
.popsection
.endm
