#pragma once

#include <stdarg.h>
#include "types.h"

extern "C" {

__noreturn
void vpanic(char const *format, va_list ap);

__noreturn
void panic(char const *format, ...)
    __printf_format(1, 2);

int vsnprintf(char *buf, size_t limit, char const *format, va_list ap);

int snprintf(char *buf, size_t limit, char const *format, ...)
    __printf_format(3, 4);

void printk(char const *format, ...)
    __printf_format(1, 2);

void vprintk(char const *format, va_list ap);

int printdbg(char const *format, ...)
    __printf_format(1, 2);

int vprintdbg(char const *format, va_list ap);

int cprintf(char const *format, ...)
    __printf_format(1, 2);

int vcprintf(char const *format, va_list ap);

int hex_dump(void const *mem, size_t size);

struct format_flag_info_t {
    char const * const name;
    int bit;
    uintptr_t mask;
    char const * const *value_names;
};

size_t format_flags_register(
        char *buf, size_t buf_size,
        uintptr_t flags, format_flag_info_t const *info);

}
