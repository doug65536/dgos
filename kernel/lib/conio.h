#pragma once

#include "dev_text.h"

extern text_dev_base_t *console_display;

int con_exists(void);
void con_clear(void);
int con_cursor_toggle(int show);
void con_putc(int character);
int con_print(char const *s);
int con_write(char const *s, intptr_t len);
void con_move_cursor(int dx, int dy);
int con_draw_xy(int x, int y, char const *s, int attr);
