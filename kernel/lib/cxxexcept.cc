#include "cxxexcept.h"
#include "printk.h"
#include "stdlib.h"

void abort()
{
    panic("abort called");
}

void __cxa_throw_bad_array_new_length()
{
    panic("bad array new length");
}

void *__cxa_allocate_exception(size_t thrown_size)
{
    return malloc(thrown_size);
}


void __cxa_free_exception(void *thrown_exception)
{
    free(thrown_exception);
}

_Unwind_Reason_Code __gxx_personality_v0(
        int version, _Unwind_Action actions, uint64_t exceptionClass,
        _Unwind_Exception *unwind_exception, _Unwind_Context *context)
{

    return _URC_CONTINUE_UNWIND;
}
