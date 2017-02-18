#pragma once
#include "types.h"

typedef struct __exception_jmp_buf_t __exception_jmp_buf_t;

struct __exception_jmp_buf_t {
    void *rip;
    void *rsp;
    uintptr_t rbx;
    uintptr_t rbp;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
    uintptr_t rflags;
};

__attribute__((returns_twice))
int __exception_setjmp(__exception_jmp_buf_t *__ctx);

__attribute__((noreturn))
int __exception_longjmp(__exception_jmp_buf_t *__ctx, int value);
