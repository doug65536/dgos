#include "syscall.h"

long syscall1(long p0, long code)
{
    __asm__ __volatile__ (
        "syscall\n\t"
        : "+a" (code),
          "+D" (p0)
        :
        : "memory", "rcx", "rdx", "rsi", "r8", "r9", "r10", "r11"
    );
    return code;
}
