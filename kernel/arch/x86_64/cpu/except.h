#pragma once
#include "types.h"
#include "except_asm.h"

struct __exception_context_t;

extern "C" long __exception_handler_remove(void);
extern "C" long __exception_handler_invoke(long exception_code);

__exception_jmp_buf_t *__exception_handler_add(void);

#define __try if (__exception_setjmp(__exception_handler_add()) == 0)

#define __catch if (__exception_handler_remove() >= 0)
#define __catch_code(__code) if ((__code = __exception_handler_remove()) >= 0)
