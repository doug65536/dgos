.code64

#include <sys/cpu_bug.h>

.section .init_array, "a"
.quad __crt0_fpu64

.section .text, "ax"

__crt0_fpu64:
    fnstcw -8(%esp)
    // PC bit 9:8 == 0x2 to set 53 bit (double) precision
    andw $~0x300,-8(%esp)
    orw $0x200,-8(%esp)
    fldcw -8(%esp)
    ret
