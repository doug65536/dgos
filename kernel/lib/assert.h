#pragma once
#include "likely.h"

void assert_failed(char const *expr,
                  char const *file,
                  int line);

#define assert(e) \
    (likely(e) ? (void)0 : assert_failed(#e, __FILE__, __LINE__))

#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
