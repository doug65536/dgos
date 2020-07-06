#include <stdio.h>
#include "bits/formatter.h"

#include <string.h>

/// emit_chars callback takes null pointer and a character,
/// or, a pointer to null terminated string and a 0
/// or, a pointer to an unterminated string and a length

int vfprintf(FILE *stream, char const *format, va_list ap)
{
    return formatter(format, ap, __emit_stream_chars, stream);
}
