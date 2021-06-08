#pragma once
#include "types.h"

struct __exception_jmp_buf_t;

struct __exception_jmp_buf_t {
    uintptr_t daif;
    void *sp;
    uintptr_t r30, r29;
    uintptr_t r28, r27;
    uintptr_t r26, r25;
    uintptr_t r24, r23;
    uintptr_t r22, r21;
    uintptr_t r20, r19;
};

extern "C" _returns_twice
intptr_t __setjmp(__exception_jmp_buf_t *__ctx);

extern "C" _noreturn
void __longjmp(__exception_jmp_buf_t *__ctx, intptr_t value);

extern "C" _noreturn
void __exception_longjmp_unwind(__exception_jmp_buf_t *__ctx, intptr_t value);
