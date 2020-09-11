#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

#ifdef __x86_64__
// All 7 callee saved registers, plus rip
#define __JMPBUF_REG_COUNT  8
#elif defined(__i386__)
// All 4 callee saved registers, plus eip
#define __JMPBUF_REG_COUNT  6
#endif

struct __jmp_buf {
    uintptr_t regs[__JMPBUF_REG_COUNT];
};

typedef __jmp_buf jmp_buf[1];

__attribute__((__returns_twice__))
int setjmp(jmp_buf env);

__attribute__((__noreturn__))
void longjmp(jmp_buf env, int value);

__END_DECLS
