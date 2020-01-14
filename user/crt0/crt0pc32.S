.code64

#include <sys/cpu_bug.h>

.section .init_array, "a"
.quad __crt0_fpu32

.section .text, "ax", @progbits

__crt0_fpu32:
    fnstcw -8(%rsp)
    // PC bit 9:8 == 0x0 to set 24 bit (float) precision
    andw $~0x300,-8(%rsp)
    fldcw -8(%rsp)
    ret
