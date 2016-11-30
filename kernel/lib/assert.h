#pragma once

void assert_failed(char const *expr,
                  char const *file,
                  int line);

#define assert(e) \
    ((e) ? (void)0 : assert_failed(#e, __FILE__, __LINE__))
