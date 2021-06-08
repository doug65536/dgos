.code64

#include <sys/cpu_bug.h>

.section .init_array, "a"
PTRSZ_DATA __crt0_fpu64

.section .text, "ax", @progbits

__crt0_fpu64:
#if defined(__x86_64__)
    fnstcw -2(RSP)
    // PC bit 9:8 == 0x2 to set 53 bit (double) precision
    andw $ ~0x300,-2(RSP)
    orw $ 0x200,-2(RSP)
    fldcw -2(RSP)
#endif
    ret
