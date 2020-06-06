#pragma once

#if !defined(NO_RETPOLINE) && \
    !defined(USE_RETPOLINE) && \
    !defined(USE_AMD_RETPOLINE)
// Guess AMD retpoline
#define USE_AMD_RETPOLINE 1
#endif

#ifdef __ASSEMBLER__
#if defined(USE_AMD_RETPOLINE)
.macro indirect_call reg
    lfence
    call *%\reg
.endm
.macro indirect_call_mem reg operand
    lfence
    callq *\operand
.endm
.macro indirect_jmp reg
    lfence
    jmpq *%\reg
.endm
.macro indirect_jmp_mem reg operand
    lfence
    jmpq *\operand
.endm
#elif defined(USE_RETPOLINE)
.macro indirect_call reg
    call __x86_indirect_thunk_\reg
.endm
.macro indirect_call_mem reg operand
    movq \operand,%\reg
    call __x86_indirect_thunk_\reg
.endm
.macro indirect_jmp reg
    jmp __x86_indirect_thunk_\reg
.endm
.macro indirect_jmp_mem reg operand
    movq \operand,%\reg
    jmp __x86_indirect_thunk_\reg
.endm
#else
.macro indirect_call reg
    callq *%\reg
.endm
.macro indirect_call_mem reg operand
    callq *operand
.endm
.macro indirect_jmp reg
    jmpq *%\reg
.endm
.macro indirect_jmp_mem reg operand
    jmpq *\operand
.endm
#endif
#endif
