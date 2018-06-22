#pragma once
#include "types.h"
#include "likely.h"

extern "C" _noinline void cpu_debug_break();

extern "C" _noinline int assert_failed(
        tchar const *expr, tchar const *msg, tchar const *file, int line);

#ifndef NDEBUG
// Plain assert
#define assert(e) \
    (likely(e) ? 1 : assert_failed(TSTR #e, 0, TSTR __FILE__, __LINE__))

// Assert with message
#define assert_msg(e, msg) \
    (likely(e) ? 1 : assert_failed(#e, (msg), TSTR(__FILE__), __LINE__))
#else
#define assert(e) (1)
#define assert_msg(e, msg) (1)
#endif

#define C_ASSERT(e) static_assert(e, #e)
