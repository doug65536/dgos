#include "progressbar.h"
#include "screen.h"

void progress_bar_draw(int top, int left, int right, int percent)
{
    print_box(left, top, right, top + 2, 0x1F, false);

    int filled = (percent * ((right - left) - 1)) / 100;
    int cleared = (right - left) - 1 - filled;
    print_xy(left + 1, top + 1, '-', 0x1F, filled);
    print_xy(left + filled + 1, top + 1, ' ', 0x1F, cleared);
}
