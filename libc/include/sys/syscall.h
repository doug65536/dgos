#pragma once

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdarg.h>

__BEGIN_DECLS

#define SYSCALL_API

// SysCall Parameter and SysCall Number
typedef int64_t scp_t;
typedef int32_t scn_t;

#ifdef __x86_64__

static inline SYSCALL_API scp_t syscall6(
        scp_t p0, scp_t p1, scp_t p2,
        scp_t p3, scp_t p4, scp_t p5, scn_t num)
{
    register scp_t rdi __asm__("rdi") = p0;
    register scp_t rsi __asm__("rsi") = p1;
    register scp_t rdx __asm__("rdx") = p2;
    register scp_t r10 __asm__("r10") = p3;
    register scp_t r8 __asm__("r8") = p4;
    register scp_t r9 __asm__("r9") = p5;
    register scp_t rax __asm__("rax") = num;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+r" (rdi)
        , "+r" (rsi)
        , "+r" (rdx)
        , "+r" (r10)
        , "+r" (r8)
        , "+r" (r9)
        , "+r" (rax)
        :
        : "rcx", "r11", "memory"
    );

    return rax;
}

static inline SYSCALL_API scp_t syscall5(
        scp_t p0, scp_t p1, scp_t p2,
        scp_t p3, scp_t p4, scn_t num)
{
    register scp_t rdi __asm__("rdi") = p0;
    register scp_t rsi __asm__("rsi") = p1;
    register scp_t rdx __asm__("rdx") = p2;
    register scp_t r10 __asm__("r10") = p3;
    register scp_t r8 __asm__("r8") = p4;
    register scp_t rax __asm__("rax") = num;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+r" (rdi)
        , "+r" (rsi)
        , "+r" (rdx)
        , "+r" (r10)
        , "+r" (r8)
        , "+r" (rax)
        :
        : "rcx", "r11", "r9", "memory"
    );

    return rax;
}

static inline SYSCALL_API scp_t syscall4(
        scp_t p0, scp_t p1, scp_t p2,
        scp_t p3, scn_t num)
{
    register scp_t rdi __asm__("rdi") = p0;
    register scp_t rsi __asm__("rsi") = p1;
    register scp_t rdx __asm__("rdx") = p2;
    register scp_t r10 __asm__("r10") = p3;
    register scp_t rax __asm__("rax") = num;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+r" (rdi)
        , "+r" (rsi)
        , "+r" (rdx)
        , "+r" (r10)
        , "+r" (rax)
        :
        : "rcx", "r11", "r9", "r8", "memory"
    );

    return rax;
}

static inline SYSCALL_API scp_t syscall3(
        scp_t p0, scp_t p1, scp_t p2, scn_t num)
{
    register scp_t rdi __asm__("rdi") = p0;
    register scp_t rsi __asm__("rsi") = p1;
    register scp_t rdx __asm__("rdx") = p2;
    register scp_t rax __asm__("rax") = num;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+r" (rdi)
        , "+r" (rsi)
        , "+r" (rdx)
        , "+r" (rax)
        :
        : "rcx", "r11", "r9", "r8", "r10", "memory"
    );

    return rax;
}

static inline SYSCALL_API scp_t syscall2(
        scp_t p0, scp_t p1, scn_t num)
{
    register scp_t rdi __asm__("rdi") = p0;
    register scp_t rsi __asm__("rsi") = p1;
    register scp_t rax __asm__("rax") = num;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+r" (rdi)
        , "+r" (rsi)
        , "+r" (rax)
        :
        : "rcx", "r11", "r9", "r8", "r10", "rdx", "memory"
    );

    return rax;
}

static inline SYSCALL_API scp_t syscall1(
        scp_t p0, scn_t num)
{
    register scp_t rdi __asm__("rdi") = p0;
    register scp_t rax __asm__("rax") = num;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+r" (rdi)
        , "+r" (rax)
        :
        : "rcx", "r11", "r9", "r8", "r10", "rdx", "rsi", "memory"
    );

    return rax;
}

static inline SYSCALL_API scp_t syscall0(
        scn_t num)
{
    register scp_t rax __asm__("rax") = num;

    __asm__ __volatile__ (
        "syscall\n\t"
        : "+r" (rax)
        :
        : "rcx", "r11", "r9", "r8", "r10", "rdx", "rsi", "rdi", "memory"
    );

    return rax;
}

#else

// Syscall number last so most of the arguments are already in the correct
// registers, and only rcx and r11 and rax need to be dealt with.
// These are implemented in assembly to ensure that link time optimizations
// will assume that all call clobbered context is clobbered.
SYSCALL_API scp_t syscall6(scp_t p0, scp_t p1,
                           scp_t p2, scp_t p3, scp_t p4, scp_t p5, scn_t num);
SYSCALL_API scp_t syscall5(scp_t p0,
                           scp_t p1, scp_t p2, scp_t p3, scp_t p4, scn_t num);
SYSCALL_API scp_t syscall4(scp_t p0, scp_t p1, scp_t p2, scp_t p3, scn_t num);
SYSCALL_API scp_t syscall3(scp_t p0, scp_t p1, scp_t p2, scn_t num);
SYSCALL_API scp_t syscall2(scp_t p0, scp_t p1, scn_t num);
SYSCALL_API scp_t syscall1(scp_t p0, scn_t num);
SYSCALL_API scp_t syscall0(scn_t num);

#endif

static _always_inline scp_t syscallv(scn_t number, va_list ap)
{
    scp_t p0 = va_arg(ap, scp_t);
    scp_t p1 = va_arg(ap, scp_t);
    scp_t p2 = va_arg(ap, scp_t);
    scp_t p3 = va_arg(ap, scp_t);
    scp_t p4 = va_arg(ap, scp_t);
    scp_t p5 = va_arg(ap, scp_t);
    return syscall6(p0, p1, p2, p3, p4, p5, number);
}

static _always_inline scp_t syscalln(scn_t number, ...)
{
    va_list ap;
    va_start(ap, number);
    scp_t result = syscallv(number, ap);
    va_end(ap);

    return result;
}

__END_DECLS

#if 0 //def __cplusplus
static _always_inline scp_t syscall(scp_t p0, scp_t p1, scp_t p2, scp_t p3,
                     scp_t p4, scp_t p5, scn_t num)
{
    return syscall6(p0, p1, p2, p3, p4, p5, num);
}

static _always_inline scp_t syscall(scp_t p0, scp_t p1, scp_t p2, scp_t p3,
                     scp_t p4, scn_t num)
{
    return syscall5(p0, p1, p2, p3, p4, num);
}

static _always_inline scp_t syscall(scp_t p0, scp_t p1, scp_t p2, scp_t p3,
                     scn_t num)
{
    return syscall4(p0, p1, p2, p3, num);
}

static _always_inline scp_t syscall(scp_t p0, scp_t p1, scp_t p2, scn_t num)
{
    return syscall3(p0, p1, p2, num);
}

static _always_inline scp_t syscall(scp_t p0, scp_t p1, scn_t num)
{
    return syscall2(p0, p1, num);
}

static _always_inline scp_t syscall(scp_t p0, scn_t num)
{
    return syscall1(p0, num);
}

static _always_inline scp_t syscall(scn_t num)
{
    return syscall0(num);
}
#endif

