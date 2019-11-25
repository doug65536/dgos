.code64

#include <sys/cpu_bug.h>

.section .init_array, "a"
.quad __crt0_fpu80

.section .text, "ax", @progbits

__crt0_fpu80:
    fnstcw -8(%esp)
    // PC bit 9:8 == 0x3 to set 64 bit (long double) precision
    orw $0x300,-8(%esp)
    fldcw -8(%esp)
    ret
