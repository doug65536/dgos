#include "except.h"
#include "stdlib.h"
#include "except_asm.h"
#include "thread.h"

struct __exception_context_t {
    __exception_jmp_buf_t __exception_state;
    int __exception_code;
    __exception_context_t *__next;
};

__exception_jmp_buf_t *__exception_handler_add(void)
{
    __exception_context_t *ctx = malloc(sizeof(*ctx));
    ctx->__next = thread_set_exception_top(ctx);
    ctx->__exception_code = -1;
    return &ctx->__exception_state;
}

int __exception_handler_remove(void)
{
    __exception_context_t *top = thread_get_exception_top();
    int code = top->__exception_code;
    thread_set_exception_top(top->__next);
    free(top);
    return code;
}

int __exception_handler_invoke(int exception_code)
{
    __exception_context_t *top = thread_get_exception_top();
    if (!top)
        return 0;

    top->__exception_code = exception_code;

    __exception_longjmp(&top->__exception_state, 1);
}
