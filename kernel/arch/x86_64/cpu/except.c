#include "except.h"
#include "stdlib.h"
#include "except_asm.h"

struct __exception_context_t {
    __exception_jmp_buf_t __exception_state;
    int __exception_code;
    __exception_context_t *__next;
};

__thread __exception_context_t *__exception_context_top;

__exception_jmp_buf_t *__exception_handler_add(void)
{
    __exception_context_t *ctx = malloc(sizeof(*ctx));
    ctx->__next = __exception_context_top;
    __exception_context_top = ctx;
    ctx->__exception_code = -1;
    return &ctx->__exception_state;
}

int __exception_handler_remove(void)
{
    int code = __exception_context_top->__exception_code;
    __exception_context_t *save = __exception_context_top;
    __exception_context_top = __exception_context_top->__next;
    free(save);
    return code;
}

int __exception_handler_invoke(int exception_code)
{
    if (!__exception_context_top)
        return 0;

    __exception_context_top->__exception_code = exception_code;

    __exception_longjmp(&__exception_context_top->__exception_state, 1);
}
