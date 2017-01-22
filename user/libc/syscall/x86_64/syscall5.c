#include "syscall.h"

long syscall5(long p0, long p1, long p2,
              long p3, long p4, long code)
{
    register long r10 asm("r10") = p3;
    register long r8 asm("r8") = p4;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+a" (code),
          "+D" (p0), "+S" (p1), "+d" (p2), "+r" (r10), "+r" (r8)
        :
        : "memory", "rcx", "r9", "r11"
    );
    return code;
}
