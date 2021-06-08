#include "printk.h"
#include "types.h"
#include "halt.h"
#include "string.h"
#include "conio.h"
#include "debug.h"
#include "mutex.h"
#include "export.h"
#include "assert.h"
#include "math.h"
#include "inttypes.h"
#include "debug.h"

debug_out_t dbgout;

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
    arg_type_double_value,
    arg_type_long_double_value
} arg_type_t;

union arg_t {
    char const *char_ptr_value;
    wchar_t const *wchar_ptr_value;
    int character;
    intptr_t intptr_value;
    uintptr_t uintptr_value;
    double double_value;
    long double long_double_value;
};

struct formatter_flags_t {
    unsigned left_justify : 1;
    unsigned leading_plus : 1;
    unsigned leading_zero : 1;
    unsigned hash : 1;
    unsigned upper : 1;
    unsigned negative : 1;
    unsigned has_min_width : 1;
    unsigned has_precision : 1;
    unsigned limit_string : 1;
    unsigned scientific : 1;
    unsigned :22;

    int min_width;
    int precision;
    int base;
    int pending_leading_zeros;
    int pending_padding;
    int max_chars;

    length_mod_t length;
    arg_t arg;
    arg_type_t arg_type;
};

static formatter_flags_t const empty_formatter_flags = {};

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

#ifndef __DGOS_KERNEL__
_const
static long double ipowl(int base, int exponent)
{
    long double result = 1;
    long double b = base;
    int neg_exp = (exponent < 0);

    // Make exponent absolute
    if (neg_exp)
        exponent = -exponent;

    while (exponent)
    {
        if (exponent & 1)
            result *= b;

        exponent >>= 1;
        b *= b;
    }

    if (neg_exp)
        result = 1.0L / result;

    return result;
}

C_ASSERT(sizeof(int64_t) == sizeof(double));

static char *dtoa(char *txt, size_t txt_sz,
                  long double n, formatter_flags_t *flags)
{
    txt[txt_sz-1] = 0;

    // Make -1 or 1 to save sign
    int sign = ((n >= 0) << 1) - 1;

    // Make n absolute
    if (sign < 0) {
        n = -n;
        flags->negative = 1;
    }

    char *out = txt;

    int exp2 = ilogbl(n);

    if (unlikely(exp2 == INT_MAX)) {
        char const *special;

        if (isinf(n))
            special = "inf";
        else if (isnan(n))
            special = "nan";
        else
            special = "???";

        strcpy(out, special);

        return txt;
    } else if (exp2 == INT_MIN) {
        exp2 = 0;
    }

    // log2(10) = 3.32192809488736
    int exp10 = exp2 / 3.32192809488736234787L;

    int scientific;
    if (exp10 < -9 || exp10 > 9)
        scientific = 1;
    else
        scientific = flags->scientific;

    int sciexp = 0;
    if (scientific) {
        // Normalize to -2 < n < -2
        if (exp10 < 0)
            --exp10;

        n /= ipowl(10, exp10);

        sciexp = exp10;
        exp10 = 0;
    } else if (exp10 < 0) {
        exp10 = 0;
    }

    // Round to precision
    long double precision = ipowl(10, -flags->precision - 1);
    //precision = nextafterl(precision, HUGE_VALL);
    precision += precision / 2.0L;
    n += 5.0L * precision;

    int exp_limit = -flags->precision - 1;

    for ( ; (exp10 >= 0 || exp10 > exp_limit); --exp10) {
        long double w = ipowl(10, exp10);
        if (w > 0) {
            int digit = n / w;
            n -= digit * w;
            *out++ = '0' + digit;
        }
        if (exp10 == 0 && n > 0)
            *out++ = '.';

        if (out + 1 >= txt + txt_sz)
            return 0;
    }

    *out = 0;

    char etxt[7];

    if (scientific) {
        char esign;
        int epad0;

        char *eout = etxt + countof(etxt);
        *--eout = 0;

        if (sciexp > 0) {
            esign = '+';
        } else {
            esign = '-';
            sciexp = -sciexp;
        }

        // The exponent must be at least two digits
        epad0 = (sciexp < 10);

        exp10 = 0;
        while (sciexp > 0) {
            *--eout = '0' + (sciexp % 10);
            sciexp /= 10;
            ++exp10;
        }

        if (epad0)
            *--eout = '0';

        *--eout = esign;
        *--eout = 'e';

        size_t elen = ((etxt + countof(etxt)) - eout) - 1;

        if (out + elen >= txt + txt_sz)
            out = txt + txt_sz - elen - 1;

        memcpy(out, eout, elen + 1);
        out += elen;
    }

    size_t len = out - txt;

    if (flags->has_min_width && (int)len < flags->min_width) {
        flags->pending_padding = flags->min_width - len;

        if (flags->pending_padding > 0) {
            if (flags->leading_zero && (flags->leading_plus || flags->negative))
                --flags->pending_padding;

            //if (flags->leading_plus &&
            //        (flags->leading_zero && flags->negative))
            //    --flags->pending_padding;

            if (flags->leading_zero) {
                flags->pending_leading_zeros = flags->pending_padding;
                flags->pending_padding = 0;
            }
        }
    }


    return txt;
}
#endif

#ifndef __COMPAT_ENHANCED
#define RETURN_FORMATTER_ERROR(chars_written) return (~chars_written)
#else
#define RETURN_FORMATTER_ERROR(chars_written) return (-1)
#endif

static char const formatter_hexlookup[] = "0123456789abcdef0123456789ABCDEF";

using formatter_lock_type = ext::spinlock;
using formatter_scoped_lock = ext::unique_lock<formatter_lock_type>;
formatter_lock_type formatter_lock;

/// emit_chars callback takes null pointer and a character,
/// or, a pointer to null terminated string and a 0
/// or, a pointer to an unterminated string and a length
intptr_t formatter(
        char const *format, va_list ap,
        int (*emit_chars)(char const *, intptr_t, void*),
        void *emit_context)
{
    formatter_flags_t flags;
    intptr_t chars_written = 0;
    int *output_arg;
    char digits[32], *digit_out;

    for (char const *fp = format; *fp; ++fp) {
        char ch = *fp;

        // Look for a span of literal characters
        if (ch != '%') {
            size_t literal_len = strcspn(fp, "%");

            chars_written += emit_chars(fp, literal_len, emit_context);
            fp += literal_len - 1;
            continue;
        }

        // At '%'
        assert(ch == '%');

        flags = empty_formatter_flags;

        ch = *++fp;

        if (ch == '%') {
            // Literal %
            chars_written += emit_chars(nullptr, '%', emit_context);
            continue;
        }

        // Consume and apply one or more -, +, #, 0 modifiers
        for (;; ch = *++fp) {
            switch (ch) {
            case '-':
                // Left justify
                flags.left_justify = 1;
                continue;

            case '+':
                // Use leading plus if positive
                flags.leading_plus = 1;
                continue;

            case '#':
                // Varies
                flags.hash = 1;
                continue;

            case '0':
                // Use leading zeros
                flags.leading_zero = 1;
                continue;
            }

            break;
        }

        // Consume and apply field width specification, if any
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

        // Consume and apply precision specification, if any
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
                               ? nullptr
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

        // Consume and apply length modifier
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

        // Consume and apply type specifier
        switch (ch) {
        case 'c':
            switch (flags.length) {
            case length_l:
                flags.arg_type = arg_type_character;
                flags.arg.character = va_arg(ap, int);
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
                if (!flags.arg.wchar_ptr_value) {
                    if (!flags.has_precision || flags.precision >= 6) {
                        flags.arg.wchar_ptr_value = L"(null)";
                    } else {
                        flags.arg.wchar_ptr_value = L"";
                    }
                }
                break;
            case length_none:
                flags.arg_type = arg_type_char_ptr;
                flags.arg.char_ptr_value = va_arg(ap, char *);
                if (!flags.arg.char_ptr_value) {
                    if (!flags.has_precision || flags.precision >= 6) {
                        flags.arg.char_ptr_value = "(null)";
                    } else {
                        flags.arg.char_ptr_value = "";
                    }
                }
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
                        va_arg(ap, uintptr_t);
                break;
            default:
                RETURN_FORMATTER_ERROR(chars_written);
            }
            break;

        case 'p':
            flags.base = 16;
            flags.hash = 1;
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

#ifndef __DGOS_KERNEL__
        case 'e':
            flags.scientific = 1;
            // fall through
        case 'f':
            if (!flags.has_precision) {
                flags.has_precision = 1;
                flags.precision = 6;
            }

            switch (flags.length) {
            case length_none:
            case length_l:
                flags.arg_type = arg_type_double_value;
                flags.arg.double_value = va_arg(ap, double);
                break;

            case length_L:
                flags.arg_type = arg_type_long_double_value;
                flags.arg.long_double_value = va_arg(ap, long double);
                break;

            default:
                RETURN_FORMATTER_ERROR(chars_written);
            }

            break;
#endif
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
            if (!flags.limit_string) {
                len = strlen(flags.arg.char_ptr_value);
            } else {
                char const *end = (char const *)
                        memchr(flags.arg.char_ptr_value, 0, flags.max_chars);
                if (end)
                    len = end - flags.arg.char_ptr_value;
                else
                    len = flags.max_chars;
            }

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

            if (flags.arg.uintptr_value == 0)
                flags.hash = 0;

            digit_out = digits + sizeof(digits);
            *--digit_out = 0;
            do {
                *--digit_out = formatter_hexlookup[
                        (flags.upper << 4) +
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

#ifndef __DGOS_KERNEL__
        case arg_type_double_value:
            dtoa(digits, sizeof(digits),
                 (long double)flags.arg.double_value, &flags);
            flags.arg_type = arg_type_char_ptr;
            flags.arg.char_ptr_value = digits;
            break;
#endif

        }

        // Make room for minus/plus
        if (flags.negative || flags.leading_plus)
            flags.pending_padding -= (flags.pending_padding > 0);

        if (flags.pending_padding != 0 &&
                !flags.left_justify) {
            // Reduce padding by number of leading zeros
            if (flags.pending_padding > flags.pending_leading_zeros)
                flags.pending_padding -= flags.pending_leading_zeros;
            else
                flags.pending_padding = 0;

            // Write leading padding for right justification
            for (int i = 0; i < flags.pending_padding; ++i)
                chars_written += emit_chars(nullptr, ' ', emit_context);
        }

        if (flags.hash && flags.base == 16)
            chars_written += emit_chars(!flags.upper ? "0x" : "0X",  2,
                                        emit_context);
        else if (flags.hash && flags.base == 8)
            chars_written += emit_chars(nullptr, '0', emit_context);

        if (flags.negative) {
            flags.negative = 0;
            chars_written += emit_chars(nullptr, '-', emit_context);
        } else if (flags.leading_plus) {
            flags.leading_plus = 0;
            chars_written += emit_chars(nullptr, '+', emit_context);
        }

        // Write leading zeros
        for (int i = 0; i < flags.pending_leading_zeros; ++i)
            chars_written += emit_chars(nullptr, '0', emit_context);

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
                                nullptr, flags.arg.char_ptr_value[i],
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
                            nullptr, flags.arg.wchar_ptr_value[i],
                            emit_context);
            break;

        case arg_type_character:
            chars_written += emit_chars(
                        nullptr, flags.arg.character,
                        emit_context);
            break;

        }

        // Write trailing padding for left justification
        if (flags.left_justify) {
            for (int i = 0; i < flags.pending_padding; ++i)
                chars_written += emit_chars(nullptr, ' ', emit_context);
        }
    }

    return chars_written;
}

int cprintf(char const * restrict format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vcprintf(format, ap);
    va_end(ap);
    return result;
}

static int vcprintf_emit_chars(char const *s, intptr_t c, void *unused)
{
    (void)unused;


    if (con_exists()) {
        if (s) {
            if (c) {
                writedbg(s, c);
                return con_write(s, c);
            }

            writedbg(s, strlen(s));

            return con_print(s);
        } else if (c) {
            char ch = (char)c;
            writedbg(&ch, 1);
            con_putc(c);
            return 1;
        }
    }
    return 0;
}

static ext::noirq_lock<ext::spinlock> cprintf_lock;

int vcprintf(char const * restrict format, va_list ap)
{
//    ext::unique_lock<ext::noirq_lock<ext::spinlock>>
//            hold_cprintf_lock(cprintf_lock);

    int chars_written = 0;
    if (con_exists()) {
        int cursor_was_shown = con_cursor_toggle(0);

        chars_written = formatter(format, ap,
                                  vcprintf_emit_chars, nullptr);

        con_cursor_toggle(cursor_was_shown);
    } else {
        vprintdbg(format, ap);
    }
    return chars_written;
}

void printk(char const *format, ...)
{
    va_list ap;
//    va_start(ap, format);
//    vprintdbg(format, ap);
//    va_end(ap);
    va_start(ap, format);
    vprintk(format, ap);
    va_end(ap);
}

void vprintk(char const * restrict format, va_list ap)
{
    vcprintf(format, ap);
}

_noreturn
void panic(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vpanic(format, ap);
    va_end(ap);
}

_noreturn
void panic_oom()
{
    panic("Out of memory");
}

_noreturn
void vpanic(char const * restrict format, va_list ap)
{
    printk("KERNEL PANIC! ");
    vprintk(format, ap);
    cpu_debug_break();
    thread_panic_other_cpus();
    halt_forever();
}

struct vsnprintf_context_t {
    char *buf;
    size_t limit;
    size_t level;
};

static int vsnprintf_emit_chars(char const * restrict s,
                                intptr_t ch, void * restrict context)
{
    vsnprintf_context_t *ctx = reinterpret_cast<vsnprintf_context_t*>(context);
    char buf[5];

    intptr_t len = 0;
    if (!s) {
        len = ucs4_to_utf8(buf, ch);
        s = buf;
    } else if (!ch) {
        len = strlen(s);
    } else {
        len = ch;
    }

    for (intptr_t i = 0; i < len; ++i) {
        if (ctx->limit && ctx->level >= ctx->limit)
            break;

        if (ctx->limit)
            ctx->buf[ctx->level++] = s[i];
    }

    return len;
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
int vsnprintf(char * restrict buf, size_t limit,
                     char const * restrict format, va_list ap)
{
    vsnprintf_context_t context;
    context.buf = buf;
    context.limit = limit;
    context.level = 0;

    intptr_t chars_needed = formatter(
                format, ap,
                vsnprintf_emit_chars, &context);

    if ((size_t)chars_needed < limit && limit)
        buf[chars_needed] = 0;
    else if (limit)
        buf[limit-1] = 0;

    return chars_needed;
}

int snprintf(char * restrict buf, size_t limit,
                    char const * restrict format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vsnprintf(buf, limit, format, ap);
    va_end(ap);
    return result;
}

int putsdbg(char const *s)
{
    size_t len = strlen(s);
    write_debug_str(s, len);
    write_debug_str("\n", 1);
    return len + 1;
}

int writedbg(char const * restrict s, size_t len)
{
    return write_debug_str(s, intptr_t(len));
}

static int printdbg_emit_chars(char const * restrict s,
                               intptr_t ch, void * restrict context)
{
    (void)context;

    char encoded[5];

    if (!s) {
        // UTF8 encode the char32_t in ch
        ch = ucs4_to_utf8(encoded, ch);
        s = encoded;
    } else if (ch == 0) {
        ch = strlen(s);
    }

    return write_debug_str(s, ch);
}

int vprintdbg(char const * restrict format, va_list ap)
{
    return formatter(format, ap, printdbg_emit_chars, nullptr);
}

int printdbg(char const * restrict format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vprintdbg(format, ap);
    va_end(ap);
    return result;
}

static int hex_dump_formatter(void const volatile *mem,
                              size_t size, uintptr_t base,
                              int (*output)(char const *format, ...)
                              _printf_format(1, 2))
{
    uint8_t volatile *buf = (uint8_t volatile *)mem;
    char line_buf[16 + 1 + 1 + 3*16 + 1 + 16 + 2 + 4];
    int written = 0;

    int line_ofs = 0;
    for (size_t i = 0; (i < size) || (i & 15); ++i) {
        if (!(i & 15)) {
            line_ofs = snprintf(line_buf, sizeof(line_buf),
                                "%08zx:", i + base);
        }

        if ((i & 0xF) && !(i & 3)) {
            line_buf[line_ofs++] = ' ';
            line_buf[line_ofs] = 0;
        }

        if (i < size) {
            line_ofs += snprintf(line_buf + line_ofs,
                                 sizeof(line_buf) - line_ofs, " %02x",
                                 buf[i]);
        } else {
            line_ofs += snprintf(line_buf + line_ofs,
                                 sizeof(line_buf) - line_ofs, "   ");
        }

        if ((i & 15) == 15) {
            line_buf[line_ofs++] = ' ';

            for (int k = -15; k <= 0; ++k) {
                line_buf[line_ofs++] =
                        (i + k) < size
                        ? (buf[i + k] < ' ' || buf[i + k] > 126
                        ? '.' : buf[i + k])
                        : ' ';
            }

            line_buf[line_ofs++] = '\n';
            line_buf[line_ofs++] = 0;
            int write_size = output("%s", line_buf);
            if (write_size < 0)
                return -1;
            written += write_size;
        }
    }
    return written;
}

int hex_dump(void const volatile *mem, size_t size, uintptr_t base)
{
    return hex_dump_formatter(mem, size, base, printdbg);
}

size_t format_flags_register(
        char *buf, size_t buf_size,
        uintptr_t flags, format_flag_info_t const *info)
{
    size_t total_written = 0;
    int chars_needed;
    bool measure = !buf_size;

    for (format_flag_info_t const *fi = info; fi->name; ++fi) {
        uintptr_t value = (flags >> fi->bit) & fi->mask;

        if (value != 0 ||
                (fi->value_names && fi->value_names[0])) {
            char const *prefix = total_written > 0 ? " " : "";
            if (fi->value_names && fi->value_names[value]) {
                // Text value
                chars_needed = snprintf(
                            buf + total_written,
                            measure ? 0 : buf_size - total_written,
                            "%s%s=%s", prefix, fi->name,
                            fi->value_names[value]);
            } else if (fi->mask == 1) {
                // Single bit flag
                chars_needed = snprintf(
                            buf + total_written,
                            measure ? 0 : buf_size - total_written,
                            "%s%s", prefix, fi->name);
            } else {
                // Multi-bit flag
                chars_needed = snprintf(
                            buf + total_written,
                            measure ? 0 : buf_size - total_written,
                            "%s%s=%#" PRIx64, prefix, fi->name, value);
            }
            if (!measure && chars_needed + total_written >= buf_size) {
                if (buf_size > 3) {
                    // Truncate with ellipsis
                    strcpy(buf + buf_size - 4, "...");
                    return buf_size - 1;
                } else if (buf_size > 0) {
                    // Truncate with * fill
                    memset(buf, '*', buf_size-1);
                    buf[buf_size-1] = 0;
                    return buf_size - 1;
                }
                // Wow, no room for anything
                return 0;
            }

            total_written += chars_needed;
        }
    }

    if (!measure)
        buf[total_written] = 0;

    return total_written;
}

debug_out_t &debug_out_t::operator<<(debug_out_modifier_t modifier)
{
    switch (modifier) {
    case dec:
        use_hex = false;
        break;

    case hex:
        use_hex = true;
        break;

    case plus:
        use_plus = true;
        break;

    case noplus:
        use_plus = false;
        break;

    }
    return *this;
}

debug_out_t &debug_out_t::write_unsigned(uint64_t value, size_t sz)
{
    printdbg(use_hex ? "%#" PRIx64
            : use_plus ? "+%" PRIu64
            : "%" PRIu64, value);
    return *this;
}

debug_out_t &debug_out_t::write_signed(int64_t value, size_t sz)
{
    printdbg(use_hex ? "%#" PRIx64
            : use_plus ? "%+" PRId64
            : "%" PRId64, value);
    return *this;
}

debug_out_t &debug_out_t::write_str(char const *rhs)
{
    printdbg("%s", rhs);
    return *this;
}

debug_out_t &debug_out_t::write_ptr(void const *rhs)
{
    printdbg("%#zx", uintptr_t(rhs));
    return *this;
}

debug_out_t &debug_out_t::write_char(char rhs)
{
    printdbg("%c", rhs);
    return *this;
}

debug_out_t &debug_out_t::write_bool(bool value)
{
    return write_str(value ? "true" : "false");
}
