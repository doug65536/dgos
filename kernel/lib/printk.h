#pragma once

#include <stdarg.h>
#include "types.h"

__BEGIN_DECLS

_noreturn
void vpanic(char const *format, va_list ap);

_noreturn
void panic(char const *format, ...)
    _printf_format(1, 2);

_noreturn
void panic_oom();

int vsnprintf(char *buf, size_t limit, char const *format, va_list ap);

int snprintf(char *buf, size_t limit, char const *format, ...)
    _printf_format(3, 4);

void printk(char const *format, ...)
    _printf_format(1, 2);

void vprintk(char const *format, va_list ap);

int printdbg(char const *format, ...)
    _printf_format(1, 2);

int vprintdbg(char const *format, va_list ap);

int cprintf(char const *format, ...)
    _printf_format(1, 2);

int vcprintf(char const *format, va_list ap);

int hex_dump(void const volatile *mem, size_t size, uintptr_t base = 0);

struct format_flag_info_t {
    char const * const name;
    uintptr_t mask;
    char const * const *value_names;
    int8_t bit;
    uint64_t :56;
};

size_t format_flags_register(
        char *buf, size_t buf_size,
        uintptr_t flags, format_flag_info_t const *info);

intptr_t formatter(
        char const *format, va_list ap,
        int (*emit_chars)(char const *, intptr_t, void*),
        void *emit_context);

__END_DECLS
