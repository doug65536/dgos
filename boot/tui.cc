#include "tui.h"
#include "screen.h"
#include "string.h"
#include "tui_scancode.h"

static char const tui_numpad[] = {
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

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

// Wait for a key
int waitkey()
{
    while (!pollkey())
    {
        //pollmouse();
    }
    return readkey();
}

void tui_menu_renderer_t::interact_timeout(int ms)
{
    if (ms < 54)
        return;

    int selection = 0;
    draw(selection);

    int ms_ticks = ms / 54;

    if (!wait_input(ms))
        return;

    // Wait timeout seconds for a keystroke
    int ticks_last = systime();
    int ticks_elap = 0;

    do {
        int ticks_this = systime();

        //*(int volatile*)0xb8000 = ticks_this;

        // Discard wraparound
        if (ticks_this <= ticks_last)
            continue;

        ticks_elap += ticks_this - ticks_last;
        ticks_last = ticks_this;

        // Check for timeout
        if (ticks_elap > ms_ticks)
            return;
    } while (!pollkey());

    //*(uint16_t volatile*)0xb8000 = '!' | 0x700;

    for (;; draw(selection)) {
        int key = waitkey();

        tui_menu_item_t *menuitem;

        switch (key & 0xFF) {
        case 0x00:
            key >>= 8;
            if (key >= key_num_first && key <= key_num_last) {
                key = tui_numpad[key - key_num_first];
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
