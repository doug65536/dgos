#pragma once

// Text display interface

#include "dev_registration.h"

typedef struct text_display_t text_display_t;

typedef struct text_display_vtbl_t text_display_vtbl_t;

typedef struct text_display_base_t {
    text_display_vtbl_t *vtbl;
} text_display_base_t;

struct text_display_vtbl_t {
    int (*detect)(text_display_base_t **result);

    // Startup/shutdown
    void (*init)(text_display_base_t *);
    void (*cleanup)(text_display_base_t *);

    // Set/get dimensions
    int (*set_dimensions)(text_display_base_t *,
                          int width, int height);
    void (*get_dimensions)(text_display_base_t *,
                           int *width, int *height);

    // Set/get cursor position
    void (*goto_xy)(text_display_base_t *,
                    int x, int y);
    int (*get_x)(text_display_base_t *);
    int (*get_y)(text_display_base_t *);

    // Set/get foreground color
    void (*fg_set)(text_display_base_t *,
                   int color);
    int (*fg_get)(text_display_base_t *);

    // Set/get background color
    void (*bg_set)(text_display_base_t *,
                   int color);
    int (*bg_get)(text_display_base_t *);

    // Show/hide cursor
    void (*cursor_toggle)(text_display_base_t *,
                          int show);
    int (*cursor_is_shown)(text_display_base_t *);

    // Print character, updating cursor position
    // possibly scrolling the screen content up
    void (*putc)(text_display_base_t *,
                int character);
    void (*putc_xy)(text_display_base_t *,
                    int x, int y, int character);

    // Print string, updating cursor position
    // possibly scrolling the screen content up
    int (*print)(text_display_base_t *,
                 char const *s);
    int (*print_xy)(text_display_base_t *,
                    int x, int y, char const *s);

    // Draw text, no effect on cursor
    int (*draw)(text_display_base_t *,
                char const *s);
    int (*draw_xy)(text_display_base_t *,
                   int x, int y, char const *s);

    // Fill an area with a character, and optionally a color
    void (*fill)(text_display_base_t *,
                 int sx, int sy,
                 int ex, int ey,
                 int character);

    // Fill screen with spaces in the current colors
    void (*clear)(text_display_base_t *);

    // Scroll an area horizontally and/or vertically
    // Optionally clearing exposed area
    void (*scroll)(text_display_base_t *,
                   int sx, int sy,
                   int ex, int ey,
                   int xd, int yd,
                   int clear);
};

#define MAKE_text_display_VTBL(name) { \
    name##_detect,                \
    name##_init,                  \
    name##_cleanup,               \
    name##_set_dimensions,        \
    name##_get_dimensions,        \
    name##_goto_xy,               \
    name##_get_x,                 \
    name##_get_y,                 \
    name##_fg_set,                \
    name##_fg_get,                \
    name##_bg_set,                \
    name##_bg_get,                \
    name##_cursor_toggle,         \
    name##_cursor_is_shown,       \
    name##_putc,                  \
    name##_putc_xy,               \
    name##_print,                 \
    name##_print_xy,              \
    name##_draw,                  \
    name##_draw_xy,               \
    name##_fill,                  \
    name##_clear,                 \
    name##_scroll                 \
}

void register_text_display_device(char const *name,
                                  text_display_vtbl_t *vtbl);

#define DECLARE_text_display_DEVICE(name) \
    DECLARE_DEVICE(text_display, name)

#define REGISTER_text_display_DEVICE(name) \
    REGISTER_DEVICE(text_display, name)

#define TEXT_DEV_PTR(dev) DEVICE_PTR(text_display, dev)

#define TEXT_DEV_PTR_UNUSED(dev) (void)dev
