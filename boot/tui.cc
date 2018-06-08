#include "tui.h"
#include "screen.h"
#include "string.h"

void tui_menu_renderer_t::measure()
{
    max_title = 0;
    max_value = 0;

    for (int i = 0; i < items->count; ++i) {
        int len = (*items)[i].title.len;
        if (max_title < len)
            max_title = len;
        for (int v = 0; v < (*items)[i].options.count; ++v) {
            len = (*items)[i].options[v].len;
            if (max_value < len)
                max_value = len;
        }
    }
}

tui_menu_renderer_t::tui_menu_renderer_t(tui_list_t<tui_menu_item_t> *items)
    : items(items)
{
}

void tui_menu_renderer_t::center()
{
    measure();

    int width = 2 + max_title + 1 + max_value + 2;
    int height = 2 + items->count + 2;

    left = 40 - (width >> 1);
    top = 13 - (height >> 1);
    resize(width, height);
}

void tui_menu_renderer_t::position(int x, int y)
{
    int width = 2 + max_title + 1 + max_value + 2;
    int height = 2 + items->count + 2;

    left = x;
    top = y;
    resize(width, height);
}

void tui_menu_renderer_t::resize(int width, int height)
{
    right = left + width;
    bottom = top + height;
}

void tui_menu_renderer_t::draw(int selection)
{
    print_box(left, top, right, bottom, 7, true);
    for (int i = 0; i < items->count; ++i) {
        auto item = (*items)[i];
        auto attr = i == selection ? 0x70 : 0x07;
        print_str_xy(left + 2, top + 2 + i,
                     item.title, item.title, attr, max_title + 1);
        print_str_xy(left + 2 + max_title + 1, top + 2 + i,
                item.options[item.index],
                item.options[item.index], attr, max_value);
    }
}

// Returns scancode in ah, ascii in al
//       esc -> 0x011B
//       '0' -> 0x0B30
//       '1' -> 0x0231
//       'A' -> 0x1E41
//       'a' -> 0x1E61
//     enter -> 0x1C2D
// num enter -> 0xE00D
//       '+' -> 0x0D2B
//     num + -> 0x4E2B
//       '-' -> 0x4A2D
//     num - -> 0x4A2D
//       '/' -> 0x352F
//     num / -> 0xE02F
//       '*' -> 0x092A
//     num * -> 0x372A

enum bioskey_e0_t {
    key_up    = 0x48E0,
    key_down  = 0x50E0,
    key_left  = 0x4BE0,
    key_right = 0x4DE0,
    key_ins   = 0x52E0,
    key_home  = 0x47E0,
    key_pgup  = 0x49E0,
    key_del   = 0x53E0,
    key_end   = 0x4FE0,
    key_pgdn  = 0x51E0
};

enum bioskey_00_t {
    key_num_7 = 0x4700,
    key_num_8 = 0x4800,
    key_num_9 = 0x4900,
    key_min_sub = 0x4A00,
    key_num_4 = 0x4B00,
    key_num_5 = 0x4C00,
    key_num_6 = 0x4D00,
    key_min_plu = 0x4E00,
    key_num_1 = 0x4F00,
    key_num_2 = 0x5000,
    key_num_3 = 0x5100,
    key_num_0 = 0x5200,
    key_num_dot = 0x5300,
    key_F1 = 0x3B00,
    key_F10 = 0x4400,
    key_F11 = 0x8500,
    key_F12 = 0x8600,

    key_num_first = key_num_7,
    key_num_last = key_num_dot
};

static char const bios_numpad[] = {
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

// Wait for a key
int waitkey()
{
    while (!pollkey());
    return readkey();
}

void tui_menu_renderer_t::interact_timeout(int ms)
{
    if (ms < 54)
        return;

    int selection = 0;
    draw(selection);

    int ms_ticks = ms / 54;

    // Wait timeout seconds for a keystroke
    int ticks_last = systime();
    int ticks_elap = 0;
    do {
        int ticks_this = systime();

        *(int volatile*)0xb8000 = ticks_this;

        // Discard wraparound
        if (ticks_this <= ticks_last)
            continue;

        ticks_elap += ticks_this - ticks_last;
        ticks_last = ticks_this;

        // Check for timeout
        if (ticks_elap > ms_ticks)
            return;
    } while (!pollkey());

    *(uint16_t volatile*)0xb8000 = '!' | 0x700;

    for (;; draw(selection)) {
        int key = waitkey();

        tui_menu_item_t *menuitem;

        switch (key & 0xFF) {
        case 0x00:
            key >>= 8;
            if (key >= key_num_first && key <= key_num_last) {
                key = bios_numpad[key - key_num_first];
                break;
            }
            break;

        case 0xE0:
            switch (key) {
            case key_up:
                if (selection > 0)
                    --selection;
                else
                    selection = items->count - 1;

                continue;

            case key_down:
                if (++selection >= items->count)
                    selection = 0;

                continue;

            case key_left:
                menuitem = &(*items)[selection];
                if (menuitem->index > 0)
                    --menuitem->index;
                break;

            case key_right:
                menuitem = &(*items)[selection];
                if (menuitem->index + 1 < menuitem->options.count)
                    ++menuitem->index;
                break;

            case key_F10:
                // Same as esc
                key = '\x1b';
                break;

            }
            break;

        default:
            key &= 0xFF;
            break;

        }

        // Exit on esc
        if (key == '\x1b')
            break;
    }
}
