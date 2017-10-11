#pragma once

// Text display interface

#include "dev_registration.h"

struct text_display_t;

struct text_display_vtbl_t;

struct text_display_base_t {
    text_display_vtbl_t *vtbl;
};

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
    int (*cursor_toggle)(text_display_base_t *,
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
    int (*write)(text_display_base_t *,
                 char const *s, intptr_t len);
    int (*print_xy)(text_display_base_t *,
                    int x, int y, char const *s);

    // Draw text, no effect on cursor
    int (*draw)(text_display_base_t *,
                char const *s);
    int (*draw_xy)(text_display_base_t *,
                   int x, int y,
                   char const *s, int attrib);

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

    // Returns 0 if mouse is not supported
    int (*mouse_supported)(text_display_base_t *);

    int (*mouse_is_shown)(text_display_base_t *);

    // Get mouse position
    int (*mouse_get_x)(text_display_base_t *);
    int (*mouse_get_y)(text_display_base_t *);

    // Move mouse to position
    void (*mouse_goto_xy)(text_display_base_t *,
                         int x, int y);

    // Move mouse by relative amount
    void (*mouse_add_xy)(text_display_base_t *,
                         int x, int y);

    // Show/hide mouse
    int (*mouse_toggle)(text_display_base_t *,
                        int show);
};

#define MAKE_text_display_VTBL(type, name) { \
    name##_detect,              \
    name##_init,                \
    name##_cleanup,             \
    name##_set_dimensions,      \
    name##_get_dimensions,      \
    name##_goto_xy,             \
    name##_get_x,               \
    name##_get_y,               \
    name##_fg_set,              \
    name##_fg_get,              \
    name##_bg_set,              \
    name##_bg_get,              \
    name##_cursor_toggle,       \
    name##_cursor_is_shown,     \
    name##_putc,                \
    name##_putc_xy,             \
    name##_print,               \
    name##_write,               \
    name##_print_xy,            \
    name##_draw,                \
    name##_draw_xy,             \
    name##_fill,                \
    name##_clear,               \
    name##_scroll,              \
    name##_mouse_supported,     \
    name##_mouse_is_shown,      \
    name##_mouse_get_x,         \
    name##_mouse_get_y,         \
    name##_mouse_goto_xy,       \
    name##_mouse_add_xy,        \
    name##_mouse_toggle         \
}

void register_text_display_device(char const *name,
                                  text_display_vtbl_t *vtbl);

#define DECLARE_text_display_DEVICE(name) \
    DECLARE_DEVICE(text_display, name)

#define REGISTER_text_display_DEVICE(name) \
    REGISTER_DEVICE(text_display, name, callout_type_t::early_dev)

#define TEXT_DEV_PTR(dev) DEVICE_PTR(text_display, dev)

#define TEXT_DEV_PTR_UNUSED(dev) (void)dev
