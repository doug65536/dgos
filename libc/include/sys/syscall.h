#pragma once
#include <sys/cdefs.h>
#include <stdarg.h>

__BEGIN_DECLS

// Syscall number last so most of the arguments are already in the correct
// registers, and only rcx and r11 and rax need to be dealt with.
// These are implemented in assembly to ensure that link time optimizations
// will assume that all call clobbered context is clobbered.
extern long syscall6(long p0, long p1, long p2, long p3,
                     long p4, long p5, unsigned num);
extern long syscall5(long p0, long p1, long p2, long p3,
                     long p4, unsigned num);
extern long syscall4(long p0, long p1, long p2, long p3,
                     unsigned num);
extern long syscall3(long p0, long p1, long p2, unsigned num);
extern long syscall2(long p0, long p1, unsigned num);
extern long syscall1(long p0, unsigned num);
extern long syscall0(unsigned num);

static _always_inline long syscallv(long number, va_list ap)
{
    long p0 = va_arg(ap, long);
    long p1 = va_arg(ap, long);
    long p2 = va_arg(ap, long);
    long p3 = va_arg(ap, long);
    long p4 = va_arg(ap, long);
    long p5 = va_arg(ap, long);
    return syscall6(p0, p1, p2, p3, p4, p5, number);
}

static _always_inline long syscalln(int number, ...)
{
    va_list ap;
    va_start(ap, number);
    long result = syscallv(number, ap);
    va_end(ap);
    return result;
}

__END_DECLS

#if 0 //def __cplusplus
static _always_inline long syscall(long p0, long p1, long p2, long p3,
                     long p4, long p5, long num)
{
    return syscall6(p0, p1, p2, p3, p4, p5, num);
}

static _always_inline long syscall(long p0, long p1, long p2, long p3,
                     long p4, long num)
{
    return syscall5(p0, p1, p2, p3, p4, num);
}

static _always_inline long syscall(long p0, long p1, long p2, long p3,
                     long num)
{
    return syscall4(p0, p1, p2, p3, num);
}

static _always_inline long syscall(long p0, long p1, long p2, long num)
{
    return syscall3(p0, p1, p2, num);
}

static _always_inline long syscall(long p0, long p1, long num)
{
    return syscall2(p0, p1, num);
}

static _always_inline long syscall(long p0, long num)
{
    return syscall1(p0, num);
}

static _always_inline long syscall(long num)
{
    return syscall0(num);
}
#endif

