#pragma once

#ifdef __ASSEMBLER__
#ifdef USE_RETPOLINE
.macro indirect_call reg
    call __x86_indirect_thunk_\reg
.endm
.macro indirect_jmp reg
    jmp __x86_indirect_thunk_\reg
.endm
#else
.macro indirect_call reg
    call *%\reg
.endm
.macro indirect_jmp reg
    jmp *%\reg
.endm
#endif
#endif
