#include "screen.h"
#include "screen_abstract.h"

char const boxchars[] = "\xC9\xBB\xBA\xC8\xBC\xCD\xDB ";

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

void scroll_screen(uint8_t attr)
{
    bios_scroll_region(0, ((24 << 8) | 79), 1, attr);
}
