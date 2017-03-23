#include "syscall.h"

long syscall4(long p0, long p1, long p2,
              long p3, long code)
{
    register long r10 asm("r10") = p3;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+a" (code),
          "+D" (p0), "+S" (p1), "+d" (p2), "+r" (r10)
        :
        : "memory", "rcx", "r8", "r9", "r11"
    );
    return code;
}
