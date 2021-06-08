#include "screen.h"
#include <stdarg.h>
#include "debug.h"
#include "malloc.h"
#include "utf.h"
#include "likely.h"

bool screen_enabled = false;

#define clamp(v, minv, maxv) \
    ((v) < (maxv) \
    ? (v) >= (minv) ? (v) \
    : (minv) \
    : (maxv))

static void buffer_char(tchar *buf, tchar **pptr, tchar c, void *arg)
{
    int *info = (int *)arg;
    size_t len = *pptr - buf;
    if (unlikely(c == tchar(-1) || len + info[1] >= 80 || c == '\n'))
    {
        *(*pptr) = 0;

        print_at(info[0], info[1], info[2], len, buf);
        //debug_out(buf, len);
        char nl = '\n';
        //debug_out(&nl, 1);
        *pptr = buf;
    }
    if (likely(c >= 32))
        *(*pptr)++ = c;
}

static void debug_out_writer(tchar const *s, size_t sz, void *)
{
    debug_out(s, sz);
}

#define FLAG_SIGNED     0x01
#define FLAG_NEG        0x02
#define FLAG_LONGLONG   0x04
#define FLAG_LONG       0x08

char const hexlookup[] = "0123456789ABCDEF";

void write_char_dummy(tchar */*buf*/, tchar **/*pptr*/, tchar /*c*/, void *)
{
}

void write_debug_dummy(tchar const */*s*/, size_t /*sz*/, void *)
{
}

void formatter(tchar const *format, va_list ap,
               void (*write_char)(tchar *buf, tchar **pptr, tchar c, void *),
               void *write_char_arg,
               void (*write_debug)(tchar const *s, size_t sz, void *),
               void *write_debug_arg)
{
    tchar buf[81];
    tchar digit[22];
    tchar *dp;
    tchar const *s;
    tchar *out = buf;
    tchar const *p = format;
    uint32_t base;
    unsigned long long n;
    uint8_t flags;

    for (p = format; *p; ++p) {
        switch (*p) {
        default:
            write_char(buf, &out, *p, write_char_arg);
            continue;

        case '%':
            flags = 0;

            if (p[1] == 'l' && p[2] == 'l') {
                flags |= FLAG_LONGLONG;
                p += 2;
            } else if (p[1] == 'l' || p[1] == 'z' ||
                       p[1] == 't' || p[1] == 'j') {
                flags |= FLAG_LONG;
                ++p;
            }

            switch (p[1]) {
            case 'x':   // fall through
            case 'p':
                base = 16;
                break;
            case 'o':
                base = 8;
                break;
            case 'd':
                base = 10;
                flags |= FLAG_SIGNED;
                break;
            case 'u':
                base = 10;
                break;
            case 's':
                base = 0;
                break;
            case '%':
                write_char(buf, &out, p[1], write_char_arg);
                continue;
            case 0:
                // Strange % at end of format string
                // Emit it
                write_char(buf, &out, p[0], write_char_arg);
                continue;
            default:
                // Strange unrecognized placeholder
                // Emit it
                write_char(buf, &out, p[0], write_char_arg);
                write_char(buf, &out, p[1], write_char_arg);
                ++p;
                continue;
            }

            ++p;

            if (base) {
                // Read correct sized vararg and sign extend if needed
                if (flags & FLAG_SIGNED) {
                    if (flags & FLAG_LONGLONG) {
                        n = (unsigned long long)va_arg(ap, long long);
                        if ((long long)n < 0)
                            flags |= FLAG_NEG;
                    } else if (flags & FLAG_LONG) {
                        n = (unsigned long long)va_arg(ap, long);
                        if ((long)n < 0)
                            flags |= FLAG_NEG;
                    } else {
                        // Sign extend to 64 bit
                        n = (unsigned long long)va_arg(ap, int);
                        if ((int)n < 0)
                            flags |= FLAG_NEG;
                    }
                } else {
                    if (flags & FLAG_LONGLONG) {
                        n = va_arg(ap, unsigned long long);
                    } else if (flags & FLAG_LONG) {
                        n = va_arg(ap, unsigned long);
                    } else {
                        n = va_arg(ap, unsigned);
                    }
                }

                // If it is signed and it is negative
                // then emit '-' and make it positive
                if ((flags & FLAG_SIGNED) && ((long long)n < 0)) {
                    // Emit negative sign
                    write_char(buf, &out, '-', write_char_arg);

                    // Get absolute value
                    n = (unsigned long long)-(long long)n;
                }

                // Build null terminated string in digit
                dp = digit + countof(digit);
                *--dp = 0;
                if (base == 16) {
                    do
                    {
                        *--dp = hexlookup[n & 0x0F];
                        n >>= 4;
                    } while (n && dp > digit);
                } else {
                    do
                    {
                        *--dp = hexlookup[(uint32_t)n % base];
                        n = (uint32_t)n / base;
                    } while (n && dp > digit);
                }

                s = dp;

                while (*s)
                    write_char(buf, &out, *s++, write_char_arg);
            } else {
                char const *s8;
                char16_t const *s16;

                if (flags & FLAG_LONG) {
                    // wide string

                    s16 = va_arg(ap, char16_t const *);

                    if (unlikely(!s16))
                        s16 = u"{[(null)]}";

#ifdef __efi
                    while (*s16)
                        write_char(buf, &out, *s16++, write_char_arg);
#else
                    for (;;) {
                        char32_t codepoint = utf16_to_ucs4_upd(s16);

                        if (unlikely(!codepoint))
                            break;

                        char encoded[8];
                        int sz = ucs4_to_utf8(encoded, codepoint);

                        for (int i = 0; i < sz; ++i)
                            write_char(buf, &out, encoded[i], write_char_arg);
                    }
#endif

                    continue;
                } else {
                    // not wide

                    s8 = va_arg(ap, char const *);

                    if (unlikely(!s8))
                        s8 = "{[(null)]}";


#ifdef __efi
                    for (;;) {
                        char32_t codepoint = utf8_to_ucs4_upd(s8);

                        if (unlikely(!codepoint))
                            break;

                        char16_t encoded[4];
                        int sz = ucs4_to_utf16(encoded, codepoint);

                        for (int i = 0; i < sz; ++i)
                            write_char(buf, &out, encoded[i], write_char_arg);
                    }
#else
                    while (*s8)
                        write_char(buf, &out, *s8++, write_char_arg);
#endif

                    continue;
                }

                while (*s)
                    write_char(buf, &out, *s++, write_char_arg);
            }

            break;
        }
    }

    write_char(buf, &out, tchar(-1), write_char_arg);
}

void vprint_line_at(int x, int y, int attr, tchar const *format, va_list ap)
{
    int const pos[] = { x, y, attr };
    formatter(format, ap, buffer_char, (void*)pos, debug_out_writer, nullptr);
}

void print_line_at(int x, int y, int attr, tchar const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprint_line_at(x, y, attr, format, ap);
    va_end(ap);
}

void vprint_line(int attr, tchar const *format, va_list ap)
{
    if (unlikely(!screen_enabled))
        return;

    print_xy(0, 24, ' ', attr, 79);
    vprint_line_at(0, 24, attr, format, ap);
}

void print_line(tchar const *format, ...)
{
    if (unlikely(!screen_enabled))
        return;

    va_list ap;
    va_start(ap, format);
    vprint_line(0x7, format, ap);
    va_end(ap);
}

void print_xy(int x, int y, tchar ch, uint16_t attr, size_t count)
{
    if (unlikely(!screen_enabled))
        return;

    tchar lbuf[81];
    tchar *fbuf = nullptr;
    if (count > 80)
        fbuf = (tchar*)malloc(count+1);
    tchar *buf = fbuf ? fbuf : lbuf;

    for (size_t ofs = 0; ofs < count; ++ofs)
        buf[ofs] = ch;
    buf[count] = 0;
    print_at(x, y, attr, count, buf);

    if (fbuf)
        free(fbuf);
}

void print_str_xy(int x, int y, tchar const *s, size_t len,
                  uint16_t attr, size_t min_len)
{
    if (unlikely(!screen_enabled))
        return;

    tchar lbuf[81];

    tchar *fbuf = nullptr;

    size_t alloc_len = min_len;// len > min_len ? len : min_len;

    if (alloc_len > sizeof(lbuf))
        fbuf = (tchar*)malloc((alloc_len+1) * sizeof(tchar));

    tchar *buf = fbuf ? fbuf : lbuf;

    size_t ofs;
    for (ofs = 0; ofs < len && ofs < min_len; ++ ofs)
        buf[ofs] = s[ofs];

    while (ofs < min_len)
        buf[ofs++] = ' ';

    buf[ofs] = 0;

    print_at(x, y, attr, ofs, buf);

    if (fbuf)
        free(fbuf);
}

void print_lba(uint32_t lba)
{
    //PRINT("%" PRIu32 "\n", lba);
}

//void dump_regs(bios_regs_t& regs, bool show_flags)
//{
//    PRINT("eax=%" PRIx32 "\n", regs.eax);
//    PRINT("ebx=%" PRIx32 "\n", regs.ebx);
//    PRINT("ecx=%" PRIx32 "\n", regs.ecx);
//    PRINT("edx=%" PRIx32 "\n", regs.edx);
//    PRINT("esi=%" PRIx32 "\n", regs.esi);
//    PRINT("edi=%" PRIx32 "\n", regs.edi);
//    PRINT("ebp=%" PRIx32 "\n", regs.ebp);
//    PRINT(" ds=%x\n", regs.ds);
//    PRINT(" es=%x\n", regs.es);
//    PRINT(" fs=%x\n", regs.fs);
//    PRINT(" gs=%x\n", regs.gs);
//    if (show_flags)
//        PRINT("flg=%" PRIx32 "\n", regs.eflags);
//}


void print_box(int left, int top, int right, int bottom, int attr, bool clear)
{
    if (unlikely(!screen_enabled))
        return;

    int sx = clamp(left, 0, 80);
    int sy = clamp(top, 0, 25);
    int ex = clamp(right, sx, 80);
    int ey = clamp(bottom, sy, 25);

    // If clipped away to nothing, then return
    if (unlikely(sx == ex || sy == ey))
        return;

    // If top was not clipped off
    if (top == sy) {
        // If the left was not clipped off
        if (left == sx)
            print_xy(sx, sy, boxchars[TL], attr, 1);

        // Adjust the beginning and end of the horizontal line to account
        // for whether the left and/or right were clipped off
        print_xy(sx + (sx == left), top, boxchars[H], attr, (ex - sx + 1) -
                 (sx == left) - (ex == right));

        // If the right was not clipped off
        if (right == ex)
            print_xy(ex, sy, boxchars[TR], attr, 1);
    }

    for (int y = sy + (top == sy); y <= (ey - (bottom == ey)); ++y) {
        // If left was not clipped off
        if (left == sx)
            print_xy(left, y, boxchars[V], attr, 1);

        if (clear) {
            // Adjust the beginning and end of the clearing spaces to account
            // for whether the left and/or right were clipped off
            print_xy(sx + (left == sx), y, ' ', attr, (ex - sx + 1) -
                     (left == sx) - (right == ex));
        }

        // If right was not clipped off
        if (right == ex)
            print_xy(right, y, boxchars[V], attr, 1);
    }

    // If bottom was not clipped off
    if (bottom == ey) {
        // If left was not clipped off
        if (left == sx)
            print_xy(sx, ey, boxchars[BL], attr, 1);

        // Adjust the beginning and end of the horizontal line to account
        // for whether the left and/or right were clipped off
        print_xy(sx + (left == sx), bottom, boxchars[H], attr,
                 (ex - sx + 1) - (left == sx) - (right == ex));

        // If right was not clipped off
        if (right == ex)
            print_xy(ex, ey, boxchars[BR], attr, 1);
    }
}
