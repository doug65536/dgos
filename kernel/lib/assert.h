#pragma once
#include "likely.h"

__attribute__((noinline))
void assert_failed(char const *expr,
                   char const *msg,
                   char const *file,
                   int line);

// Plain assert
#define assert(e) \
    (likely(e) ? (void)0 : assert_failed(#e, 0, __FILE__, __LINE__))

// Assert with message
#define assert_msg(e, msg) \
    (likely(e) ? (void)0 : assert_failed(#e, (msg), __FILE__, __LINE__))

// Compile-time assert
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]

// Assert that a value is an integer power of two
#define C_ASSERT_ISPO2(n) C_ASSERT(((n) & ((n)-1)) == 0)
