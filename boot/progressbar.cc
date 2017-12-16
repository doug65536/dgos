#include "progressbar.h"
#include "screen.h"

// ╔╗║╚╝═█
// 01234567
static char boxchars[] = "\xC9\xBB\xBA\xC8\xBC\xCD\xDB ";

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

void progress_bar_draw(int top, int left, int right, int percent)
{
    print_xy(left, top, boxchars[TL], 0x0F, 1);
    print_xy(left + 1, top, boxchars[H], 0x0F, right - left - 1);
    print_xy(right, top, boxchars[TR], 0x0F, 1);
    print_xy(left, top + 1, boxchars[V], 0x0F, 1);
    print_xy(right, top + 1, boxchars[V], 0x0F, 1);
    print_xy(left, top + 2, boxchars[BL], 0x0F, 1);
    print_xy(left + 1, top + 2, boxchars[H], 0x0F, right - left - 1);
    print_xy(right, top + 2, boxchars[BR], 0x0F, 1);

    int filled = percent * (right - left - 1) / 100;
    int cleared = right - left - 1 - filled;
    print_xy(left + 1, top + 1, boxchars[S], 0x0F,
            filled);
    print_xy(left + filled, top + 1, boxchars[X], 0x0F,
            cleared);
}
