#include "conio.h"
#include "assert.h"
#include "printk.h"

text_dev_base_t *console_display;

void con_clear(void)
{
    console_display->clear();
}

int con_cursor_toggle(int show)
{
    return console_display->cursor_toggle(show);
}

void con_putc(int character)
{
    console_display->putc(character);
}

int con_print(char const *s)
{
    return console_display->print(s);
}

int con_write(char const *s, intptr_t len)
{
    return console_display->write(s, len);
}

void con_move_cursor(int dx, int dy)
{
    console_display->mouse_add_xy(dx, dy);
}

int con_draw_xy(int x, int y, char const *s, int attr)
{
    if (console_display) {
        return console_display->draw_xy(x, y, s, attr);
    } else {
        printdbg("Console draw too early @ %d,%d: %s\n", x, y, s);
    }
    return 0;
}

int con_exists()
{
    return console_display != nullptr;
}
