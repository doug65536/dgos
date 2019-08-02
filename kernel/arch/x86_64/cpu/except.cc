#include "except.h"
#include "stdlib.h"
#include "except_asm.h"
#include "thread.h"

__exception_jmp_buf_t *__exception_handler_add(void)
{
    __exception_context_t *ctx = new (std::nothrow) __exception_context_t;
    ctx->flags = 1;
    ctx->__next = (__exception_context_t *)thread_set_exception_top(ctx);
    ctx->__exception_code = -1;
    return &ctx->__exception_state;
}

__exception_jmp_buf_t *
__exception_handler_add_owned(__exception_context_t *ectx)
{
    *ectx = {};
    ectx->flags = 0;
    ectx->__next = (__exception_context_t *)thread_set_exception_top(ectx);
    ectx->__exception_code = -1;
    return &ectx->__exception_state;
}

long __exception_handler_remove(void)
{
    __exception_context_t *top = (__exception_context_t *)
            thread_get_exception_top();
    long code = top->__exception_code;
    thread_set_exception_top(top->__next);
    if (top->flags & 1)
        delete top;
    return code;
}

long __exception_handler_invoke(long exception_code)
{
    __exception_context_t *top = (__exception_context_t *)
            thread_get_exception_top();
    if (!top)
        return 1;

    top->__exception_code = exception_code;

    __exception_longjmp_unwind(&top->__exception_state, 1);
}
