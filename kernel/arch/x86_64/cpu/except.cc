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
    __exception_context_t *ctx = new __exception_context_t;
    ctx->__next = (__exception_context_t *)thread_set_exception_top(ctx);
    ctx->__exception_code = -1;
    return &ctx->__exception_state;
}

int __exception_handler_remove(void)
{
    __exception_context_t *top = (__exception_context_t *)
            thread_get_exception_top();
    int code = top->__exception_code;
    thread_set_exception_top(top->__next);
    free(top);
    return code;
}

int __exception_handler_invoke(int exception_code)
{
    __exception_context_t *top = (__exception_context_t *)
            thread_get_exception_top();
    if (!top)
        return 1;

    top->__exception_code = exception_code;

    __exception_longjmp_unwind(&top->__exception_state, 1);
}
