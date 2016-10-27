#include "code16gcc.h"

#include <stdarg.h>
#include "screen.h"

#define DIRECT_VGA 1
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
    uint16_t ax = 0x13 << 8;
    uint16_t bx = attr;
    uint16_t cx = length;
    uint16_t dx = col | (row << 8);
    __asm__ __volatile__ (
        "xchgw %%bp,%%si\n\t"
        "int $0x10\n\t"
        "xchgw %%bp,%%si\n\t"
        :
        : "d" (dx), "c" (cx), "b" (bx), "a" (ax), "S" (text)
        : "memory"
    );
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
    __asm__ __volatile__ (
        // Some BIOS have a bug which clobbers bp
        "pushl %%ebp\n\t"
        "int $0x10\n\t"
        "popl %%ebp\n\t"
        : "=a" (lines)
        : "c" (top_left), "d" (bottom_right), "a" (lines | (0x06 << 8)), "b" (attr << 8)
    );
}

static void bios_scroll_screen()
{
    bios_scroll_region(0, ((24 << 8) | 79), 1, 0x00);
}

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

static void buffer_char(char *buf, char **pptr, char c)
{
    if (*pptr - buf == 80 || c == '\n')
    {
        scroll_screen();
        *(*pptr) = 0;
        print_at(24, 0, 0x07, *pptr - buf, buf);
        *pptr = buf;
    }
    if (c >= 32)
        *(*pptr)++ = c;
}

void print_line(char const* format, ...)
{
    va_list ap;
    va_start(ap, format);

    char buf[81];
    char digit[12];
    char *dp;
    char const *s;
    char *out = buf;
    char const *p = format;
    uint32_t base;
    uint32_t n;
    uint16_t is_signed;
    static char const hexlookup[] = "0123456789ABCDEF";

    for (p = format; *p; ++p) {
        switch (*p) {
        case '%':
            switch (p[1]) {
            case 'x':
                base = 16;
                is_signed = 0;
                break;
            case 'd':
                base = 10;
                is_signed = 1;
                break;
            case 'u':
                base = 10;
                is_signed = 0;
                break;
            case 's':
                base = 0;
                break;
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
                n = (uint32_t)va_arg(ap, int);

                // If it is signed and it is negative
                // then emit '-' and make it positive
                if (is_signed && (int32_t)n < 0) {
                    buffer_char(buf, &out, '-');
                    n = (uint32_t)-(int32_t)n;
                }

                // Build null terminated string in digit
                dp = digit + 12;
                *--dp = 0;
                do
                {
                    *--dp = hexlookup[n % base];
                    n /= base;
                } while (n);

                s = dp;
            } else {
                s = va_arg(ap, char const*);
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
        scroll_screen();
    }

    va_end(ap);
}
