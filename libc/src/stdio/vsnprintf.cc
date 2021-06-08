#include <stdio.h>
#include <string.h>
#include "bits/formatter.h"
#include <sys/likely.h>

struct print_info_t {
    char *buf;
    size_t limit;
    size_t level;
};

/// emit_chars callback takes null pointer and a character,
/// or, a pointer to null terminated string and a 0
/// or, a pointer to an unterminated string and a length
static int __emit_string_chars(char const *p, intptr_t n, void *ctx)
{
    print_info_t *info = reinterpret_cast<print_info_t*>(ctx);

    if (!p) {
        // Put character into buffer if there is a buffer
        if (info->buf && info->limit && info->level < info->limit)
            info->buf[info->level] = n;

        // Advance number of characters we would have written,
        // regardless of whether they fit
        ++info->level;

        return 1;
    }

    if (!n)
        n = strlen(p);

    size_t copied;

    if (info->level + n < info->limit)
        copied = n;
    else if (info->level < info->limit)
        copied = info->limit - info->level;
    else
        copied = 0;

    if (info->buf)
        memcpy(info->buf + info->level, p, copied);

    info->level += copied;

    return n;
}

int vsnprintf(char *buffer, size_t buffer_sz, char const *format, va_list ap)
{
    print_info_t info{buffer, buffer_sz, 0};

    int result = formatter(format, ap, __emit_string_chars, &info);

    if (unlikely(info.buf && info.limit && result >= 0 &&
                 size_t(result) >= info.limit)) {
        result = info.limit - 1;
        info.buf[result] = 0;
    }

    return result;
}
