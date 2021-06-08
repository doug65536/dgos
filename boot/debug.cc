#include "debug.h"
#include "string.h"
#include "utf.h"
#include "serial.h"
#include "formatter.h"
#include "screen.h"
#include "likely.h"

#ifdef __x86_64__
#include "x86/cpu_x86.h"
#endif

static bool serial_ready = false;
static bool use_serial = true;

static void e9out(char const *data, size_t len)
{
#if !defined(__aarch64__)
    if (use_serial) {
        if (!serial_ready) {
            arch_serial_init(1);
            serial_ready = true;
        }

        if (serial_logger_t::instance)
            serial_logger_t::instance->write(0, (char*)data, len);
//    } else {
//        outsb(0xe9, data, len);
    }
#endif
}

void debug_out(char const *s, ptrdiff_t len)
{
    if (len < 0)
        len = strlen(s);
    e9out(s, len);
}

void debug_out(char16_t const *s, ptrdiff_t len)
{
    if (len < 0)
        len = strlen(s);

    // Convert utf-16 to utf-8

    char16_t const *end = s + len;

    // Allocate worst case UTF-8 output buffer
    char16_t const *in = s;
    char *buf = (char*)__builtin_alloca(len * 4 + 1);
    char *out = buf;
    ptrdiff_t enc_len = 0;
    while (in < end) {
        char32_t codepoint = tchar_to_ucs4_upd(in);
        enc_len += ucs4_to_utf8(out + enc_len, codepoint);
    }
    e9out(buf, enc_len);
}

int printdbg(tchar const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vprintdbg(format, ap);
    va_end(ap);
    return result;
}

// Expects 80 byte buffer. Line buffered.
static void write_debug_out(tchar *buf, tchar **pptr, tchar c, void *)
{
    tchar *&ptr = *pptr;

    if (c != tchar(-1)) {
        // Not flush
        *ptr++ = c;
        *ptr = '\n';
    }

    size_t used = ptr - buf;

    if (used >= 80 || c == '\n' || c == '\r' || c == tchar(-1)) {
        // Full or end of line or flush
        ptr = buf;

        if (likely(used))
            debug_out(buf, used + 1);
    }
}

int vprintdbg(tchar const *format, va_list ap)
{
    formatter(format, ap,
              write_debug_out, nullptr);
    return 0;
}
