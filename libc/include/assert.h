#pragma once
#include <sys/cdefs.h>

__BEGIN_DECLS

__attribute__((__noreturn__))
void __assert_failed(char const *filename, int line, char const *expr);

#define assert(e) (__builtin_expect(!!(e), !0) \
        ? (void)0 : \
        __assert_failed(__FILE__, __LINE__, #e))

__END_DECLS
