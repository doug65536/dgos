#pragma once

void __assert_failed(char const *filename, int line, char const *expr);

#define assert(e) (__builtin_expect(!!(e), !0) \
        ? (void)0 : \
        __assert_failed(__FILE__, __LINE__, #e))
