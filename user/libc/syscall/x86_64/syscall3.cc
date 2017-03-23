#include "syscall.h"

long syscall3(long p0, long p1, long p2, long code)
{
    __asm__ __volatile__ (
        "syscall\n\t"
        : "+a" (code),
          "+D" (p0), "+S" (p1), "+d" (p2)
        :
        : "memory", "rcx", "r8", "r9", "r10", "r11"
    );
    return code;
}
