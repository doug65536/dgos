#include "syscall.h"

long syscall0(long code)
{
    __asm__ __volatile__ (
        "syscall\n\t"
        : "+a" (code)
        :
        : "memory", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"
    );
    return code;
}
