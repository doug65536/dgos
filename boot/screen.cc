#include "screen.h"
#include <stdarg.h>
#include "debug.h"
#include "bioscall.h"

static void buffer_char(tchar *buf, tchar **pptr, tchar c)
{
    if (*pptr - buf == 80 || c == '\n')
    {
        scroll_screen();
        *(*pptr) = 0;
        print_at(24, 0, 0x07, *pptr - buf, buf);
        debug_out(buf, *pptr - buf);
        *pptr = buf;
    }
    if (c >= 32)
        *(*pptr)++ = c;
}

#define FLAG_SIGNED     0x01
#define FLAG_NEG        0x02
#define FLAG_LONGLONG   0x04
#define FLAG_LONG       0x08

char const hexlookup[] = "0123456789ABCDEF";

void print_line(tchar const* format, ...)
{
    va_list ap;
    va_start(ap, format);

    tchar buf[81];
    tchar digit[22];
    tchar *dp;
    tchar const *s;
    tchar *out = buf;
    tchar const *p = format;
    uint32_t base;
    uint64_t n;
    uint8_t flags;

    for (p = format; *p; ++p) {
        switch (*p) {
        case '%':
            flags = 0;

            if (p[1] == 'l' && p[2] == 'l') {
                flags |= FLAG_LONGLONG;
                p += 2;
            } else if (p[1] == 'l' || p[1] == 'z') {
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
                if (!(flags & FLAG_LONGLONG))
                    flags |= FLAG_SIGNED;
                break;
            case 'u':
                base = 10;
                break;
            case 's':
                base = 0;
                break;
            case '%':
                buffer_char(buf, &out, p[1]);
                continue;
            case 0:
                // Strange % at end of format string
                // Emit it
                buffer_char(buf, &out, p[0]);
                continue;
            default:
                // Strange unrecognized placeholder
                // Emit it
                buffer_char(buf, &out, p[0]);
                buffer_char(buf, &out, p[1]);
                ++p;
                continue;
            }

            ++p;

            if (base) {
                // Read correct sized vararg and sign extend if needed
                if (flags & FLAG_SIGNED) {
                    if (flags & FLAG_LONGLONG) {
                        n = (uint64_t)va_arg(ap, int64_t);
                        if ((int64_t)n < 0)
                            flags |= FLAG_NEG;
                    } else {
                        // Sign extend to 64 bit
                        n = (uint64_t)(int64_t)va_arg(ap, int32_t);
                        if ((int32_t)n < 0)
                            flags |= FLAG_NEG;
                    }
                } else {
                    if (flags & FLAG_LONGLONG) {
                        n = va_arg(ap, uint64_t);
                    } else {
                        n = va_arg(ap, uint32_t);
                    }
                }

                // If it is signed and it is negative
                // then emit '-' and make it positive
                if ((flags & FLAG_SIGNED) && (int64_t)n < 0) {
                    // Emit negative sign
                    buffer_char(buf, &out, '-');

                    // Get absolute value
                    n = (uint64_t)-(int64_t)n;
                }

                // Build null terminated string in digit
                dp = digit + 22;
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
            } else {
                s = va_arg(ap, tchar const*);

                if (!s)
                    s = TEXT("{[(null)]}");
            }

            while (*s)
                buffer_char(buf, &out, *s++);

            break;

        default:
            buffer_char(buf, &out, *p);
            break;
        }
    }
    if (out != buf) {
        print_at(24, 0, 0x07, out - buf, buf);
        debug_out(buf, out - buf);

        scroll_screen();
    }

    va_end(ap);
}

void print_xy(int x, int y, tchar ch, uint16_t attr, size_t count)
{
    tchar *buf = (tchar*)__builtin_alloca(count+1);
    for (size_t ofs = 0; ofs < count; ++ofs)
        buf[ofs] = ch;
    buf[count] = 0;
    print_at(y, x, attr, count, buf);
}

void print_str_xy(int x, int y, tchar const *s, size_t len,
                  uint16_t attr, size_t min_len)
{
    print_at(y, x, attr, len, s);
    if (min_len > len)
        print_xy(x + len, y, ' ', attr, min_len - len);
}

void print_lba(uint32_t lba)
{
    PRINT("%lu\n", lba);
}

void dump_regs(bios_regs_t& regs, bool show_flags)
{
    PRINT("eax=%lx\n", regs.eax);
    PRINT("ebx=%lx\n", regs.ebx);
    PRINT("ecx=%lx\n", regs.ecx);
    PRINT("edx=%lx\n", regs.edx);
    PRINT("esi=%lx\n", regs.esi);
    PRINT("edi=%lx\n", regs.edi);
    PRINT("ebp=%lx\n", regs.ebp);
    PRINT(" ds=%x\n", regs.ds);
    PRINT(" es=%x\n", regs.es);
    PRINT(" fs=%x\n", regs.fs);
    PRINT(" gs=%x\n", regs.gs);
    if (show_flags)
        PRINT("flg=%lx\n", regs.eflags);
}

// ╔╗║╚╝═█
// 01234567
static char const boxchars[] = "\xC9\xBB\xBA\xC8\xBC\xCD\xDB ";

enum boxchar_index_t {
    TL,
    TR,
    V,
    BL,
    BR,
    H,
    S,
    X
};

void print_box(int left, int top, int right, int bottom, int attr, bool clear)
{
    print_xy(left, top, boxchars[TL], attr, 1);
    print_xy(left + 1, top, boxchars[H], attr, right - left - 1);
    print_xy(right, top, boxchars[TR], attr, 1);
    for (int y = top + 1; y < bottom; ++y) {
        print_xy(left, y, boxchars[V], attr, 1);
        if (clear)
            print_xy(left + 1, y, ' ', attr, right - left - 1);
        print_xy(right, y, boxchars[V], attr, 1);
    }
    print_xy(left, bottom, boxchars[BL], attr, 1);
    print_xy(left + 1, bottom, boxchars[H], attr, right - left - 1);
    print_xy(right, bottom, boxchars[BR], attr, 1);
}
