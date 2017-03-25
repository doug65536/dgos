#pragma once

#include <stdarg.h>
#include "types.h"

extern "C" {

#ifdef __GNUC__
#define ATTRIBUTE_FORMAT(m,n) __attribute__((format(printf, m, n)))
#else
#define ATTRIBUTE_FORMAT(m,n)
#endif

__attribute__((noreturn))
void vpanic(char const *format, va_list ap);

__attribute__((noreturn))
void panic(char const *format, ...)
    ATTRIBUTE_FORMAT(1, 2);

int vsnprintf(char *buf, size_t limit, char const *format, va_list ap);

int snprintf(char *buf, size_t limit, char const *format, ...)
    ATTRIBUTE_FORMAT(3, 4);

void printk(char const *format, ...)
    ATTRIBUTE_FORMAT(1, 2);

void vprintk(char const *format, va_list ap);

void printdbg(char const *format, ...)
    ATTRIBUTE_FORMAT(1, 2);

void vprintdbg(char const *format, va_list ap);

int cprintf(char const *format, ...)
    ATTRIBUTE_FORMAT(1, 2);

int vcprintf(char const *format, va_list ap);

}
