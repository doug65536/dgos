#include "stacktrace.h"
#include <unwind.h>
#include "likely.h"

#define EH_FRAME_UNWIND 1

#if EH_FRAME_UNWIND

struct stacktrace_ctx_t {
    void **addresses;
    size_t max_frames;
    size_t index;
    stacktrace_cb_t cb;
};

static _Unwind_Reason_Code stacktrace_callback(_Unwind_Context *ctx, void *arg)
{
    stacktrace_ctx_t *trace_ctx = (stacktrace_ctx_t*)arg;

    if (unlikely(!trace_ctx->cb && trace_ctx->index >= trace_ctx->max_frames))
        return _URC_NORMAL_STOP;

    _Unwind_Ptr ip = _Unwind_GetIP(ctx);
    _Unwind_Ptr cf = _Unwind_GetCFA(ctx);

    if (trace_ctx->cb)
        trace_ctx->cb(cf, ip);
    else
        trace_ctx->addresses[trace_ctx->index++] = (void*)ip;

    return _URC_NO_REASON;
}

void stacktrace(stacktrace_cb_t cb)
{
    stacktrace_ctx_t ctx{nullptr, 0, 0, cb};
    _Unwind_Backtrace(stacktrace_callback, &ctx);
}

size_t stacktrace(void **addresses, size_t max_frames)
{
    stacktrace_ctx_t ctx{addresses, max_frames, 0, nullptr};

    _Unwind_Backtrace(stacktrace_callback, &ctx);

    return ctx.index;
}
#else
#if !defined(__x86_64__) && !defined(__i386__)
#error This code only works on x86 and x86-64
#endif

struct stack_frame_t {
    stack_frame_t *parent;
    void *return_addr;
};

size_t stacktrace(void **addresses, size_t max_frames)
{
    auto frame = (stack_frame_t const *)__builtin_frame_address(0);

    size_t count;
    for (count = 0; count < max_frames && frame;
         ++count, frame = frame->parent)
        addresses[count] = frame->return_addr;

    return count;
}
#endif
