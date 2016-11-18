#include "thread.h"

thread_t thread_create(thread_fn_t fn, void *context, size_t stack_size)
{
    (void)fn;
    (void)context;
    (void)stack_size;
    return 0;
}

