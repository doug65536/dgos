#include "printk.h"
#include "types.h"
#include "cpu/halt.h"
#include "string.h"
#include "conio.h"

typedef enum length_mod_t {
    length_none,
    length_hh,
    length_h,
    length_l,
    length_ll,
    length_j,
    length_z,
    length_t,
    length_L
} length_mod_t;

typedef enum arg_type_t {
    arg_type_none,

    arg_type_char_ptr,
    arg_type_wchar_ptr,
    arg_type_character,
    arg_type_intptr_value,
    arg_type_uintptr_value,
} arg_type_t;

typedef union arg_t {
    char *char_ptr_value;
    wchar_t *wchar_ptr_value;
    int character;
    intptr_t intptr_value;
    uintptr_t uintptr_value;
} arg_t;

typedef struct formatter_flags_t {
    unsigned int left_justify : 1;
    unsigned int leading_plus : 1;
    unsigned int leading_zero : 1;
    unsigned int hash : 1;
    unsigned int upper : 1;
    unsigned int negative : 1;
    unsigned int has_min_width : 1;
    unsigned int has_precision : 1;
    unsigned int limit_string : 1;

    int min_width;
    int precision;
    int base;
    int pending_leading_zeros;
    int pending_padding;
    int max_chars;

    length_mod_t length;
    arg_type_t arg_type;
    arg_t arg;
} formatter_flags_t;

static formatter_flags_t const empty_formatter_flags;

/// Parse an integer from a string,
/// returns the first non-digit
static char const *parse_int(char const *p, int *result)
{
    int n = 0;
    int sign = 1;

    if (*p == '-') {
        sign = -1;
        ++p;
    }

    while (*p >= '0' && *p <= '9')
        n = n * 10 + (*p++ - '0');

    if (result)
        *result = n * sign;

    return p;
}

#ifndef __COMPAT_ENHANCED
#define RETURN_FORMATTER_ERROR(chars_written) return (~chars_written)
#else
#define RETURN_FORMATTER_ERROR(chars_written) return (-1)
#endif

static char const formatter_hexlookup[] = "0123456789ABCDEF";

/// emit_chars callback takes null pointer and a character,
/// or, a pointer to null terminated string and a 0
static intptr_t formatter(
        const char *format, va_list ap,
        int (*emit_chars)(char const *, int, void*),
        void *emit_context)
{
    formatter_flags_t flags;
    intptr_t chars_written = 0;
    int *output_arg;
    char digits[32], *digit_out;

    for (char const *fp = format; *fp; ++fp) {
        char ch = *fp;

        switch (ch) {
        case '%':
            flags = empty_formatter_flags;

            ch = *++fp;

            switch (ch) {
            case '%':
                // Literal %
                chars_written += emit_chars(0, '%', emit_context);
                continue;

            case '-':
                // Left justify
                flags.left_justify = 1;
                ch = *++fp;
                break;

            case '+':
                // Use leading plus if positive
                flags.leading_plus = 1;
                ch = *++fp;
                break;

            case '#':
                // Varies
                flags.hash = 1;
                ch = *++fp;
                break;

            case '0':
                // Use leading zeros
                flags.leading_zero = 1;
                ch = *++fp;
                break;
            }

            if (ch == '*') {
                // Get minimum field width from arguments
                flags.has_min_width = 1;
                flags.min_width = va_arg(ap, int);
                ch = *++fp;
            } else if (ch >= '0' && ch <= '9') {
                // Parse numeric field width
                flags.has_min_width = 1;
                fp = parse_int(fp, &flags.min_width);
                // Leading 0 on width specifies 0 padding
                if (ch == '0')
                    flags.precision = flags.min_width;
                ch = *fp;
            }

            if (ch == '.') {
                ch = *++fp;

                if (flags.precision == 0 && ch == '*') {
                    // Get precision from arguments
                    flags.has_precision = 1;
                    flags.precision = va_arg(ap, int);
                    ch = *++fp;
                } else if (ch == '-' || (ch >= '0' && ch <= '9')) {
                    // Parse numeric precision
                    flags.has_precision = 1;
                    fp = parse_int(fp,
                                   flags.precision
                                   ? 0
                                   : &flags.precision);

                    // Negative values are ignored
                    if (flags.precision < 0)
                        flags.precision = 0;
                    ch = *fp;
                }
            }

            // If no precision specified and leading zero
            // was given on min_width, then use min_width
            // for the precision
            if (!flags.has_precision && flags.leading_zero)
                flags.precision = flags.min_width;

            // Length modifier
            switch (ch) {
            case 'h':
                if (fp[1] == 'h') {
                    flags.length = length_hh;
                    fp += 2;
                } else {
                    flags.length = length_h;
                    fp += 1;
                }
                ch = *fp;
                break;

            case 'l':
                if (fp[1] == 'l') {
                    flags.length = length_ll;
                    fp += 2;
                } else {
                    flags.length = length_l;
                    fp += 1;
                }
                ch = *fp;
                break;

            case 'j':
                flags.length = length_j;
                ch = *++fp;
                break;

            case 'z':
                flags.length = length_z;
                ch = *++fp;
                break;

            case 't':
                flags.length = length_t;
                ch = *++fp;
                break;

            case 'L':
                flags.length = length_L;
                ch = *++fp;
                break;

            }

            switch (ch) {
            case 'c':
                switch (flags.length) {
                case length_l:
                    flags.arg_type = arg_type_character;
                    flags.arg.character = va_arg(ap, wchar_t);
                    break;
                case length_none:
                    flags.arg_type = arg_type_character;
                    flags.arg.character = (char)va_arg(ap, int);
                    break;
                default:
                    RETURN_FORMATTER_ERROR(chars_written);
                }
                break;

            case 's':
                if (flags.has_precision) {
                    flags.limit_string = 1;
                    flags.max_chars = flags.precision;
                }

                switch (flags.length) {
                case length_l:
                    flags.arg_type = arg_type_wchar_ptr;
                    flags.arg.wchar_ptr_value = va_arg(ap, wchar_t *);
                    break;
                case length_none:
                    flags.arg_type = arg_type_char_ptr;
                    flags.arg.char_ptr_value = va_arg(ap, char *);
                    break;
                default:
                    RETURN_FORMATTER_ERROR(chars_written);
                }
                break;

            case 'd':
            case 'i':
                flags.base = 10;

                flags.arg_type = arg_type_intptr_value;
                switch (flags.length) {
                case length_hh:
                    // signed char
                    flags.arg.intptr_value =
                            (signed char)va_arg(ap, int);
                    break;
                case length_h:
                    // short
                    flags.arg.intptr_value =
                            (short)va_arg(ap, int);
                    break;
                case length_none:
                    // int
                    flags.arg.intptr_value =
                            va_arg(ap, int);
                    break;
                case length_l:
                    // long
                    flags.arg.intptr_value =
                            va_arg(ap, long);
                    break;
                case length_ll:
                    // long long
                    flags.arg.intptr_value =
                            va_arg(ap, long long);
                    break;
                case length_j:
                    // intmax_t
                    flags.arg.intptr_value =
                            va_arg(ap, intmax_t);
                    break;
                case length_z:
                    // signed size_t
                    flags.arg.intptr_value =
                            va_arg(ap, ssize_t);
                    break;
                case length_t:
                    // ptrdiff_t
                    flags.arg.intptr_value =
                            va_arg(ap, ptrdiff_t);
                    break;
                default:
                    RETURN_FORMATTER_ERROR(chars_written);
                }
                break;

            case 'X':
            case 'x':
            case 'u':
            case 'o':
                flags.upper = (ch >= 'A' && ch <= 'Z');
                flags.base = (ch == 'o' ? 8 : ch == 'u' ? 10 : 16);

                flags.arg_type = arg_type_uintptr_value;
                switch (flags.length) {
                case length_hh:
                    // unsigned signed char
                    flags.arg.uintptr_value =
                            (unsigned char)va_arg(ap, unsigned int);
                    break;

                case length_h:
                    // unsigned short
                    flags.arg.uintptr_value =
                            (unsigned short)va_arg(ap, unsigned int);
                    break;

                case length_none:
                    // unsigned int
                    flags.arg.uintptr_value =
                            va_arg(ap, unsigned int);
                    break;

                case length_l:
                    // unsigned long
                    flags.arg.uintptr_value =
                            va_arg(ap, unsigned long);
                    break;

                case length_ll:
                    // unsigned long long
                    flags.arg.uintptr_value =
                            va_arg(ap, unsigned long long);
                    break;

                case length_j:
                    // uintmax_t
                    flags.arg.uintptr_value =
                            va_arg(ap, uintmax_t);
                    break;

                case length_z:
                    // ssize_t
                    flags.arg.uintptr_value =
                            va_arg(ap, size_t);
                    break;

                case length_t:
                    // uptrdiff_t
                    flags.arg.uintptr_value =
                            va_arg(ap, uptrdiff_t);
                    break;
                default:
                    RETURN_FORMATTER_ERROR(chars_written);
                }
                break;

            case 'p':
                flags.base = 16;
                switch (flags.length) {
                case length_none:
                    flags.arg_type = arg_type_uintptr_value;
                    flags.arg.uintptr_value =
                            (uintptr_t)va_arg(ap, void*);
                    break;

                default:
                    RETURN_FORMATTER_ERROR(chars_written);
                }
                break;

            case 'n':
                switch (flags.length) {
                case length_none:
                    output_arg = va_arg(ap, int *);
                    *output_arg = chars_written;
                    break;

                default:
                    RETURN_FORMATTER_ERROR(chars_written);
                }
                break;
            }

            //
            // Convert digits to text then change them to strings
            // to handle padding later

            int len;

            switch (flags.arg_type) {
            default:
                break;

            case arg_type_none:
                // Nothing to do!
                continue;

            case arg_type_character:
                if (flags.min_width > 1)
                    flags.pending_padding = flags.min_width - 1;
                break;

            case arg_type_char_ptr:
                len = strlen(flags.arg.char_ptr_value);
                if (flags.limit_string && len > flags.precision)
                    len = flags.precision;

                if (flags.min_width > len)
                    flags.pending_padding = flags.min_width - len;

                break;

            case arg_type_intptr_value:
                if (flags.arg.intptr_value < 0) {
                    flags.negative = 1;
                    flags.arg.intptr_value = -flags.arg.intptr_value;
                }
                flags.arg.uintptr_value = (uintptr_t)flags.arg.intptr_value;
                // fall through
            case arg_type_uintptr_value:
                flags.pending_leading_zeros = flags.precision;
                flags.pending_padding = flags.min_width;

                digit_out = digits + sizeof(digits);
                *--digit_out = 0;
                do {
                    *--digit_out = formatter_hexlookup[
                            flags.arg.uintptr_value % flags.base];
                    flags.arg.uintptr_value /= flags.base;

                    flags.pending_leading_zeros -=
                            (flags.pending_leading_zeros > 0);
                    flags.pending_padding -=
                            (flags.pending_padding > 0);
                } while (digit_out > digits && flags.arg.uintptr_value);

                // Now treat as string
                // Don't forget to emit negative and leading zeros later
                flags.arg_type = arg_type_char_ptr;
                flags.arg.char_ptr_value = digit_out;

                break;

            }

            // Make room for minus/plus
            if (flags.negative || flags.leading_plus) {
                flags.pending_padding -=
                        (flags.pending_padding > 0);
            }

            if (flags.pending_padding != 0 &&
                    !flags.left_justify) {
                // Reduce padding by number of leading zeros
                if (flags.pending_padding > flags.pending_leading_zeros)
                    flags.pending_padding -= flags.pending_leading_zeros;
                else
                    flags.pending_padding = 0;

                // Write leading padding for right justification
                for (int i = 0; i < flags.pending_padding; ++i)
                    chars_written += emit_chars(0, ' ', emit_context);
            }

            if (flags.negative) {
                flags.negative = 0;
                chars_written += emit_chars(0, '-', emit_context);
            } else if (flags.leading_plus) {
                flags.leading_plus = 0;
                chars_written += emit_chars(0, '+', emit_context);
            }

            // Write leading zeros
            for (int i = 0; i < flags.pending_leading_zeros; ++i)
                chars_written += emit_chars(0, '0', emit_context);

            //
            // Now print stuff...

            switch (flags.arg_type) {
            default:
                // Nothing to do!
                continue;

            case arg_type_char_ptr:
                if (flags.limit_string) {
                    // Limit string output
                    for (size_t i = 0; flags.arg.char_ptr_value[i] &&
                         flags.max_chars > 0 &&
                         i < (size_t)flags.max_chars; ++i) {
                        chars_written += emit_chars(
                                    0, flags.arg.char_ptr_value[i],
                                    emit_context);
                    }
                } else {
                    chars_written += emit_chars(
                                flags.arg.char_ptr_value, 0,
                                emit_context);
                }
                break;

            case arg_type_wchar_ptr:
                for (size_t i = 0; flags.arg.wchar_ptr_value[i]; ++i)
                    chars_written += emit_chars(
                                0, flags.arg.wchar_ptr_value[i],
                                emit_context);
                break;

            case arg_type_character:
                chars_written += emit_chars(
                            0, flags.arg.character,
                            emit_context);
                break;

            }

            // Write trailing padding for left justification
            if (flags.left_justify) {
                for (int i = 0; i < flags.pending_padding; ++i)
                    chars_written += emit_chars(0, ' ', emit_context);
            }
            break;

        default:
            chars_written += emit_chars(0, ch, emit_context);
            break;

        }
    }

    return chars_written;
}

int cprintf(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vcprintf(format, ap);
    va_end(ap);
    return result;
}

static int vcprintf_emit_chars(char const *s, int c, void *unused)
{
    (void)unused;

    if (con_exists()) {
        if (s) {
            return con_print(s);
        } else if (c) {
            con_putc(c);
            return 1;
        }
    }
    return 0;
}

int vcprintf(char const *format, va_list ap)
{
    int chars_written = 0;
    if (con_exists()) {
        int cursor_was_shown = con_cursor_toggle(0);

        chars_written = formatter(format, ap,
                                      vcprintf_emit_chars, 0);

        con_cursor_toggle(cursor_was_shown);
    }
    return chars_written;
}

void printk(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprintk(format, ap);
    va_end(ap);
}

void vprintk(char const *format, va_list ap)
{
    vcprintf(format, ap);
}

void panic(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vpanic(format, ap);
    va_end(ap);
}

void vpanic(char const *format, va_list ap)
{
    printk("KERNEL PANIC! ");
    vprintk(format, ap);
    halt_forever();
}

typedef struct vsnprintf_context_t {
    char *buf;
    size_t limit;
    size_t level;
} vsnprintf_context_t;

static int vsnprintf_emit_chars(char const *s, int ch, void *context)
{
    vsnprintf_context_t *ctx = context;
    int count = 0;

    do {
        if (s) {
            ch = *s++;
            if (!ch)
                break;
        }

        ++count;

        if (ctx->level < ctx->limit - 1) {
            ctx->buf[ctx->level] = ch;
            ++ctx->level;
        }
    } while (s);

    return count;
}

// Write up to "limit" bytes to "buf" using format string.
// If limit is 0, buf is optional and is never accessed.
// Always return the number of bytes of output it would
// have generated, not including the null terminator, if
// it were given a suffiently sized buffer. May return
// a number higher than the number of bytes written to
// "buf".
// "buf" is guaranteed to be null terminated upon
// returning, if limit > 0.
int vsnprintf(char *buf, size_t limit, const char *format, va_list ap)
{
    vsnprintf_context_t context;
    context.buf = buf;
    context.limit = limit;
    context.level = 0;

    intptr_t chars_needed = formatter(
                format, ap,
                vsnprintf_emit_chars, &context);

    if ((size_t)chars_needed < limit)
        buf[chars_needed] = 0;
    else
        buf[limit-1] = 0;

    return chars_needed;
}

int snprintf(char *buf, size_t limit, char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vsnprintf(buf, limit, format, ap);
    va_end(ap);
    return result;
}
