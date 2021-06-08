#pragma once
#include "types.h"
#include "debug.h"
#include <stdarg.h>

#define DEBUG(...) printdbg(TSTR __VA_ARGS__)

_no_tstr_printf_format(1, 2)
static inline void printdbg_dummy(char const *format, ...){}

_no_tstr_printf_format(1, 0)
static inline void vprintdbg_dummy(char const *format, ...){}

_no_tstr_printf_format(1, 2)
int printdbg(tchar const *format, ...);

_no_tstr_printf_format(1, 0)
int vprintdbg(tchar const *format, va_list ap);

void debug_out(char const *s, ptrdiff_t len);
void debug_out(char16_t const *s, ptrdiff_t len);

static inline void debug_out(wchar_t const *s, ptrdiff_t len)
{
    return debug_out((char16_t*)s, len);
}
