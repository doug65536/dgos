#include "syscall.h"

long syscall2(long p0, long p1, long code)
{
    __asm__ __volatile__ (
        "syscall\n\t"
        : "+a" (code),
          "+D" (p0), "+S" (p1)
        :
        : "memory", "rcx", "rdx", "r8", "r9", "r10", "r11"
    );
    return code;
}

