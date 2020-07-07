#include "progressbar.h"
#include "screen.h"
#include "string.h"
#include "likely.h"

void progress_bar_draw(int left, int top, int right,
                       int percent, tchar const *title,
                       bool bar_only)
{
    if (title) {
        size_t title_len = strlen(title);

        if (!bar_only) {
            print_xy(left, top, ' ', 0xF, title_len);

            print_box(left, top, left + title_len + 3, top + 2, 0x17, true);

            print_at(left + 2, top + 1, 0x17, title_len, title);
        }

        left += title_len + 4;
    }

    if (!bar_only)
        print_box(left, top, right, top + 2, 0x18, false);

    int filled = (percent * ((right - left) - 1)) / 100;

    tchar fill_char = boxchar_solid();

    if (unlikely(filled < 0 || filled > 100)) {
        fill_char = '?';
        filled = 1;
    }

    int cleared = (right - left) - 1 - filled;

    print_xy(left + 1, top + 1, fill_char, 0x17, filled);
    print_xy(left + filled + 1, top + 1, ' ', 0x18, cleared);
}
