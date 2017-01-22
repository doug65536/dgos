#include "syscall.h"

// syscall rax
// params rdi, rsi, rdx, r10, r8, r9
// return rax

long syscall6(long p0, long p1, long p2,
              long p3, long p4, long p5, long code)
{
    register long r10 asm("r10") = p3;
    register long r8 asm("r8") = p4;
    register long r9 asm("r9") = p5;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+a" (code),
          "+D" (p0), "+S" (p1), "+d" (p2), "+r" (r10), "+r" (r8), "+r" (r9)
        :
        : "memory", "rcx", "r11"
    );
    return code;
}
