#pragma once
#include "types.h"
#include "likely.h"

extern "C" __noinline int assert_failed(
        char const *expr, char const *msg, char const *file, int line);

#ifndef NDEBUG
// Plain assert
#define assert(e) \
    (likely(e) ? 1 : assert_failed(#e, 0, __FILE__, __LINE__))

// Assert with message
#define assert_msg(e, msg) \
    (likely(e) ? 1 : assert_failed(#e, (msg), __FILE__, __LINE__))
#else
#define assert(e) (1)
#define assert_msg(e, msg) (1)
#endif

// Assert that a value is an integer power of two
#define assert_ispo2(n) \
    assert_msg(((n) & ((n)-1)) == 0, "Must be a power of two")

// Compile-time assert
#ifdef __cplusplus
#define C_ASSERT(e) static_assert(e, #e)
#else
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif

// Assert that a value is an integer power of two
#define C_ASSERT_ISPO2(n) C_ASSERT(((n) & ((n)-1)) == 0)
