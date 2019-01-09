#include <stdio.h>
#include "bits/formatter.h"

#include <string.h>

/// emit_chars callback takes null pointer and a character,
/// or, a pointer to null terminated string and a 0
/// or, a pointer to an unterminated string and a length

static int emit_stream_chars(char const *p, intptr_t n, void* ctx)
{
    FILE *stream = (FILE*)ctx;

    if (!p) {
        fputc(n, stream);
        return 1;
    }

    if (!n)
        n = strlen(p);

    fwrite(p, 1, n, stream);
    return n;
}

int vfprintf(FILE *stream, const char *format, va_list ap)
{
    return formatter(format, ap, emit_stream_chars, stream);
}
