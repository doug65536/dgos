.code64

#include <sys/cpu_bug.h>

.section .init_array, "a"
PTRSZ_DATA __crt0_fpu80

.section .text, "ax", @progbits

__crt0_fpu80:
#if defined(__x86_64__)
    fnstcw -2(RSP)
    // PC bit 9:8 == 0x3 to set 64 bit (long double) precision
    orw $ 0x300,-2(RSP)
    fldcw -2(RSP)
#endif
    ret
