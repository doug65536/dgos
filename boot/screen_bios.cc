#include "screen.h"

#define DIRECT_VGA 0

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
void print_at(int row, int col, uint8_t attr, size_t length, char const *text)
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

void scroll_screen()
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
