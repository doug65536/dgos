#include "printk.h"
#include "types.h"

void printk(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprintk(format, ap);
    va_end(ap);
}

typedef enum {
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

typedef enum {
    arg_type_none,

    arg_type_char_ptr,
    arg_type_wchar_ptr,
    arg_type_char_buf,
    arg_type_wchar_buf,
    arg_type_intptr_value,
    arg_type_uintptr_value,
} arg_type_t;

typedef union {
    char *char_ptr_value;
    wchar_t *wchar_ptr_value;
    char char_buf[2];
    wchar_t wchar_buf[2];
    intptr_t intptr_value;
    uintptr_t uintptr_value;
} arg_t;

typedef struct {
    unsigned int left_justify : 1;
    unsigned int leading_plus : 1;
    unsigned int leading_zero : 1;
    unsigned int hash : 1;
    unsigned int upper : 1;

    int min_width;
    int precision;
    int base;

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

    *result = n * sign;

    return p;
}

#ifndef __COMPAT_ENHANCED
#define RETURN_FORMATTER_ERROR(chars_written) return (~chars_written)
#else
#define RETURN_FORMATTER_ERROR(chars_written) return (-1)
#endif

/// emit_chars callback takes null pointer and a character,
/// or, a pointer to null terminated string and a 0
static int formatter(const char *format, va_list ap,
              int (*emit_chars)(char *, int))
{
    formatter_flags_t flags;
    int chars_written = 0;
    int *output_arg;

    for (char const *fp = format; *fp; ++fp) {
        char ch = *fp;

        switch (ch) {
        case '%':
            flags = empty_formatter_flags;

            ch = *++fp;

            switch (ch) {
            case '%':
                // Literal %
                chars_written += emit_chars(0, '%');
                continue;

            case '-':
                // Left justify
                flags.left_justify = 1;
                break;

            case '+':
                // Use leading plus if positive
                flags.leading_plus = 1;
                break;

            case '#':
                // Varies
                flags.hash = 1;
                break;

            case '0':
                // Use leading zeros
                flags.leading_zero = 1;
                break;
            }

            if (ch == '*') {
                // Get minimum field width from arguments
                flags.min_width = va_arg(ap, int);
            } else if (ch >= '0' && ch <= '9') {
                // Parse numeric field width
                fp = parse_int(fp, &flags.min_width);
            }

            if (ch == '.') {
                ch = *++fp;

                if (ch == '*') {
                    // Get precision from arguments
                    flags.precision = va_arg(ap, int);
                } else if (ch == '-' || (ch >= '0' && ch <= '9')) {
                    // Parse numeric precision
                    // Negative values are ignored
                    fp = parse_int(fp, &flags.precision);
                }
            }

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
                break;

            case 'l':
                if (fp[1] == 'l') {
                    flags.length = length_ll;
                    fp += 2;
                } else {
                    flags.length = length_l;
                    fp += 1;
                }
                break;

            case 'j':
                flags.length = length_j;
                ++fp;
                break;

            case 'z':
                flags.length = length_z;
                ++fp;
                break;

            case 't':
                flags.length = length_t;
                ++fp;
                break;

            case 'L':
                flags.length = length_L;
                ++fp;
                break;

            }

            switch (ch) {
            case 'c':
                switch (flags.length) {
                case length_l:
                    flags.arg_type = arg_type_wchar_buf;
                    flags.arg.wchar_buf[0] = va_arg(ap, wchar_t);
                    break;
                case length_none:
                    flags.arg_type = arg_type_char_buf;
                    flags.arg.char_buf[0] = (char)va_arg(ap, int);
                    break;
                default:
                    RETURN_FORMATTER_ERROR(chars_written);
                }
                break;

            case 's':
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
                    flags.arg.intptr_value = (signed char)va_arg(ap, int);
                    break;
                case length_h:
                    // short
                    flags.arg.intptr_value = (short)va_arg(ap, int);
                    break;
                case length_none:
                    // int
                    flags.arg.intptr_value = va_arg(ap, int);
                    break;
                case length_l:
                    // long
                    flags.arg.intptr_value = va_arg(ap, long);
                    break;
                case length_ll:
                    // long long
                    flags.arg.intptr_value = va_arg(ap, long long);
                    break;
                case length_j:
                    // intmax_t
                    flags.arg.intptr_value = va_arg(ap, intmax_t);
                    break;
                case length_z:
                    // signed size_t
                    flags.arg.intptr_value = va_arg(ap, ssize_t);
                    break;
                case length_t:
                    // ptrdiff_t
                    flags.arg.intptr_value = va_arg(ap, ptrdiff_t);
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
                    flags.arg.uintptr_value = (unsigned char)va_arg(ap, unsigned int);
                    break;

                case length_h:
                    // unsigned short
                    flags.arg.uintptr_value = (unsigned short)va_arg(ap, unsigned int);
                    break;

                case length_none:
                    // unsigned int
                    flags.arg.uintptr_value = va_arg(ap, unsigned int);
                    break;

                case length_l:
                    // unsigned long
                    flags.arg.uintptr_value = va_arg(ap, unsigned long);
                    break;

                case length_ll:
                    // unsigned long long
                    flags.arg.uintptr_value = va_arg(ap, unsigned long long);
                    break;

                case length_j:
                    // uintmax_t
                    flags.arg.uintptr_value = va_arg(ap, uintmax_t);
                    break;

                case length_z:
                    // ssize_t
                    flags.arg.uintptr_value = va_arg(ap, size_t);
                    break;

                case length_t:
                    // uptrdiff_t
                    flags.arg.uintptr_value = va_arg(ap, uptrdiff_t);
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
                    flags.arg.uintptr_value = (uintptr_t)va_arg(ap, void*);
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
            break;

        default:
            emit_chars(0, ch);
            break;

        }
    }

    return chars_written;
}
