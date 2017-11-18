#include "screen.h"
#include <stdarg.h>
#include "debug.h"
#include "bioscall.h"

#define DIRECT_VGA 0
#if DIRECT_VGA
#define print_at vga_print_at
#define scroll_screen vga_scroll_screen
#else
#define print_at bios_print_at
#define scroll_screen bios_scroll_screen
#endif

//void print_int(uint32_t n)
//{
//    char buf[12];
//    char *p;
//    p = buf + 12;
//    *--p = 0;
//    do {
//        *(--p) = '0' + (n % 10);
//        n /= 10;
//    } while (p > buf && n != 0);
//    //wtfgcc(p);
//    print_raw(1, 0, 0x0F, (buf + 12) - p, p);
//    //bochs_out(p);
//}

#if !DIRECT_VGA

// INT 0x10
// AH = 0x13
// AL = write mode
// bit 0:
// Update cursor after writing
//
// bit 1:
// String contains alternating characters and attributes
//
// bits 2-7:
// Reserved (0).
// BH = page number.
// BL = attribute if string contains only characters.
// CX = number of characters in string.
// DH,DL = row,column at which to start writing.
// ES:BP -> string to write
static void bios_print_at(
        uint8_t row, uint8_t col, uint8_t attr,
        uint16_t length, char const *text)
{
    bios_regs_t regs;

    regs.eax = 0x1300;
    regs.ebx = attr;
    regs.ecx = length;
    regs.edx = col | (row << 8);
    regs.ebp = (uint32_t)text;

    bioscall(&regs, 0x10);
}

static void bios_scroll_region(uint16_t top_left, uint16_t bottom_right,
                               uint16_t lines, uint16_t attr)
{
    // AH = 06h
    // AL = number of lines by which to scroll up (00h = clear entire window)
    // BH = attribute used to write blank lines at bottom of window
    // CH,CL = row,column of window's upper left corner
    // DH,DL = row,column of window's lower right corner
    // Int 10h

    bios_regs_t regs;

    regs.ecx = top_left;
    regs.edx = bottom_right;
    regs.eax = (lines | (0x06 << 8));
    regs.ebx = (attr << 8);

    bioscall(&regs, 0x10);
}

static void bios_scroll_screen()
{
    bios_scroll_region(0, ((24 << 8) | 79), 1, 0x00);
}

#else

static void vga_scroll_screen()
{
    uint16_t di = 0;
    uint16_t si = 80 * 2;
    uint16_t cx = 80 * 24;
    uint16_t ax = 0xb800;
    __asm__ __volatile__ (
        "pushw %%ds\n\t"
        "pushw %%es\n\t"

        "movw %%ax,%%ds\n\t"
        "movw %%ax,%%es\n\t"

        "cld\n\t"
        "rep movsw\n\t"

        "movl $80,%%ecx\n\t"
        "movl $0,%%eax\n\t"
        "rep stosw\n\t"

        "popw %%es\n\t"
        "popw %%ds\n\t"
        : "=a" (ax), "=S" (si), "=D" (di), "=c" (cx)
        : "a" (ax), "S" (si), "D" (di), "c" (cx)
    );
}

static void vga_print_at(
        uint8_t row, uint8_t col, uint8_t attr,
        uint16_t length, char const *text)
{
    uint16_t offset = (uint16_t)(row * 80 + col) << 1;
    __asm__ __volatile__ (
        "pushl %%gs\n\t"

        // point gs to physical address 0xB8000
        "movw $0xb800,%%ax\n\t"
        "movw %%ax,%%gs\n\t"

        // Loop
        "copy_to_screen_another%=:\n\t"

        // Test length remaining before each iteration
        "testw %%cx,%%cx\n\t"
        "jz copy_to_screen_done%=\n\t"
        "decw %%cx\n\t"

        // Read a byte from message and postincrement
        "movb (%%si),%%al\n\t"
        "incw %%si\n\t"

        // Write character and attribute byte and postincrement
        "gs movb %%al,(%%di)\n\t"
        "gs movb %%dl,1(%%di)\n\t"
        "addw $2,%%di\n\t"

        // Writes off the end of the screen are truncated
        "cmpw $2000,%%di\n"
        "ja copy_to_screen_another%=\n\t"

        "copy_to_screen_done%=:\n\t"

        "popl %%gs\n\t"
        : "=D" (offset), "=S" (text), "=c" (length)
        : "D" (offset), "S" (text), "c" (length), "d" (attr)
        : "eax"
    );
}
#endif

static void buffer_char(char *buf, char **pptr, char c)
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

#define USE_LONGLONG_MOD 1

#if USE_LONGLONG_MOD
// Only works correctly for values <= 2⁵³
// Which is fine for all legal addresses,
// and all reasonable file offsets
static uint64_t ulonglong_mod(uint64_t a, uint64_t b)
{
    double da = (double)a;
    double db = (double)b;
    double dq = da / db;
    uint64_t quot = (uint64_t)dq;
    return (uint64_t)(da - (db * (double)quot));
}

static uint64_t ulonglong_div(uint64_t a, uint64_t b)
{
    double da = (double)a;
    double db = (double)b;
    double dq = da / db;
    uint64_t quot = (uint64_t)dq;
    return quot;
}
#endif

char const hexlookup[] = "0123456789ABCDEF";

void print_line(char const* format, ...)
{
    va_list ap;
    va_start(ap, format);

    char buf[81];
    char digit[22];
    char *dp;
    char const *s;
    char *out = buf;
    char const *p = format;
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
            } else if (p[1] == 'l') {
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
                // We force all 64 bit values to hex,
                // if USE_LONGLONG_MOD is 0,
                // because of issues with libgcc
                // __udivi3 and __umodi3. 64 bit values are
                // almost always file offsets or addresses
                // and hex is ideal for those anyway
                if (base == 16 || (!USE_LONGLONG_MOD && (flags & FLAG_LONGLONG))) {
                    do
                    {
                        *--dp = hexlookup[n & 0x0F];
                        n >>= 4;
                    } while (n && dp > digit);
                } else {
                    do
                    {
#if USE_LONGLONG_MOD
                        *--dp = hexlookup[ulonglong_mod(n, base)];
                        n = ulonglong_div(n, base);
#else
                        *--dp = hexlookup[(uint32_t)n % base];
                        n = (uint32_t)n / base;
#endif
                    } while (n && dp > digit);
                }

                s = dp;
            } else {
                s = va_arg(ap, char const*);

                if (!s)
                    s = "{[(null)]}";
                else if ((uint32_t)s >= 0x10000)
                    s = "{[(invalid)]}";
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

void print_xy(uint16_t x, uint16_t y, uint16_t ch, uint16_t attr, uint16_t count)
{
    char *buf = (char*)__builtin_alloca(count+1);
    for (uint16_t ofs = 0; ofs < count; ++ofs)
        buf[ofs] = (char)ch;
    buf[count] = 0;
    print_at(y, x, attr, count, buf);
}

void print_lba(uint32_t lba)
{
    print_line("%lu\n", lba);
}

void dump_regs(bios_regs_t& regs, bool show_flags)
{
    print_line("eax=%lx\n", regs.eax);
    print_line("ebx=%lx\n", regs.ebx);
    print_line("ecx=%lx\n", regs.ecx);
    print_line("edx=%lx\n", regs.edx);
    print_line("esi=%lx\n", regs.esi);
    print_line("edi=%lx\n", regs.edi);
    print_line("ebp=%lx\n", regs.ebp);
    print_line(" ds=%x\n", regs.ds);
    print_line(" es=%x\n", regs.es);
    print_line(" fs=%x\n", regs.fs);
    print_line(" gs=%x\n", regs.gs);
    if (show_flags)
        print_line("flg=%lx\n", regs.eflags_out);
}
