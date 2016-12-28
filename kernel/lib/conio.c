#include "conio.h"

text_display_base_t *console_display;
text_display_vtbl_t console_display_vtbl;

void con_clear(void)
{
    console_display_vtbl.clear(console_display);
}

int con_cursor_toggle(int show)
{
    return console_display_vtbl.cursor_toggle(
                console_display, show);
}

void con_putc(int character)
{
    console_display_vtbl.putc(
                console_display, character);
}

int con_print(char const *s)
{
    return console_display_vtbl.print(
                console_display, s);
}

int con_write(char const *s, intptr_t len)
{
    return console_display_vtbl.write(
                console_display, s, len);
}

void con_move_cursor(int dx, int dy)
{
    console_display_vtbl.mouse_add_xy(
                console_display, dx, dy);
}

int con_draw_xy(int x, int y, char const *s, int attr)
{
    return console_display_vtbl.draw_xy(
                console_display, x, y, s, attr);
}

int con_exists()
{
    return console_display != 0;
}
