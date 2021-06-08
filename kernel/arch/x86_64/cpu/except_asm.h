#pragma once
#include "types.h"

struct __exception_jmp_buf_t;

struct __exception_jmp_buf_t {
    void *ip;
    void *sp;
    uintptr_t rbx;
    uintptr_t rbp;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
    uintptr_t rflags;
};

extern "C" _returns_twice
intptr_t __setjmp(__exception_jmp_buf_t *__ctx);

extern "C" _noreturn
void __longjmp(__exception_jmp_buf_t *__ctx, intptr_t value);

extern "C" _noreturn
void __exception_longjmp_unwind(__exception_jmp_buf_t *__ctx, intptr_t value);
