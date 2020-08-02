.code64

#include <sys/cpu_bug.h>

.section .init_array, "a"
PTRSZ_DATA __crt0_fpu32

.section .text, "ax", @progbits

__crt0_fpu32:
    fnstcw -2(RSP)
    // x87 PC bit 9:8 == 0x0 to set 24 bit (float) precision
    andw $ ~0x300,-2(RSP)
    // SSE PC bit
    fldcw -2(RSP)
    ret
