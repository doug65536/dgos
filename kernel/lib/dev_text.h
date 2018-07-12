#pragma once

// Text display interface

#include "dev_registration.h"
#include "vector.h"

struct text_dev_factory_t;
struct text_dev_base_t;

void register_text_display_device(
        char const *name, text_dev_factory_t *factory);

struct text_dev_factory_t {
    text_dev_factory_t(char const *name)
        : name(name)
    {
        register_text_display_device(name, this);
    }

    virtual int detect(text_dev_base_t*** ptr_list) = 0;

    char const *name;

    // Text devices start so early, memory allocation is not available, so
    // generate a linked list of factories instead:
    text_dev_factory_t *next_factory;
};

struct text_dev_base_t {
    // Set/get dimensions
    // Startup/shutdown
    virtual void init() = 0;
    virtual void cleanup() = 0;

    // Set/get dimensions
    virtual int set_dimensions(int width, int height) = 0;
    virtual void get_dimensions(int *width, int *height) = 0;

    // Set/get cursor position
    virtual void goto_xy(int x, int y) = 0;
    virtual int get_x() = 0;
    virtual int get_y() = 0;

    // Set/get foreground color
    virtual void fg_set(int color) = 0;
    virtual int fg_get() = 0;

    // Set/get background color
    virtual void bg_set(int color) = 0;
    virtual int bg_get() = 0;

    // Show/hide cursor
    virtual int cursor_toggle(int show) = 0;
    virtual int cursor_is_shown() = 0;

    // Print character, updating cursor position
    // possibly scrolling the screen content up
    virtual void putc(int character) = 0;
    virtual void putc_xy(int x, int y, int character) = 0;

    // Print string, updating cursor position
    // possibly scrolling the screen content up
    virtual int print(char const *s) = 0;
    virtual int write(char const *s, intptr_t len) = 0;
    virtual int print_xy(int x, int y, char const *s) = 0;

    // Draw text, no effect on cursor
    virtual int draw(char const *s) = 0;
    virtual int draw_xy(int x, int y, char const *s, int attrib) = 0;

    // Fill an area with a character, and optionally a color
    virtual void fill(int sx, int sy, int ex, int ey, int character) = 0;

    // Fill screen with spaces in the current colors
    virtual void clear() = 0;

    // Scroll an area horizontally and/or vertically
    // Optionally clearing exposed area
    virtual void scroll(int sx, int sy, int ex, int ey,
                        int xd, int yd, int clear) = 0;

    // Returns 0 if mouse is not supported
    virtual int mouse_supported() = 0;

    virtual int mouse_is_shown() = 0;

    // Get mouse position
    virtual int mouse_get_x() = 0;
    virtual int mouse_get_y() = 0;

    // Move mouse to position
    virtual void mouse_goto_xy(int x, int y) = 0;

    // Move mouse by relative amount
    virtual void mouse_add_xy(int x, int y) = 0;

    // Show/hide mouse
    virtual int mouse_toggle(int show) = 0;
};

#define TEXT_DEV_IMPL                                                       \
    virtual void init() override final;                                     \
    virtual void cleanup() override final;                                  \
    virtual int set_dimensions(int width, int height) override final;       \
    virtual void get_dimensions(int *width, int *height) override final;    \
    virtual void goto_xy(int x, int y) override final;                      \
    virtual int get_x() override final;                                     \
    virtual int get_y() override final;                                     \
    virtual void fg_set(int color) override final;                          \
    virtual int fg_get() override final;                                    \
    virtual void bg_set(int color) override final;                          \
    virtual int bg_get() override final;                                    \
    virtual int cursor_toggle(int show) override final;                     \
    virtual int cursor_is_shown() override final;                           \
    virtual void putc(int character) override final;                        \
    virtual void putc_xy(int x, int y, int character) override final;       \
    virtual int print(char const *s) override final;                        \
    virtual int write(char const *s, intptr_t len) override final;          \
    virtual int print_xy(int x, int y, char const *s) override final;       \
    virtual int draw(char const *s) override final;                         \
    virtual int draw_xy(int x, int y,                                       \
            char const *s, int attrib) override final;                      \
    virtual void fill(int sx, int sy,                                       \
            int ex, int ey, int character) override final;                  \
    virtual void clear() override final;                                    \
    virtual void scroll(int sx, int sy, int ex, int ey,                     \
                        int xd, int yd, int clear) override final;          \
    virtual int mouse_supported() override final;                           \
    virtual int mouse_is_shown() override final;                            \
    virtual int mouse_get_x() override final;                               \
    virtual int mouse_get_y() override final;                               \
    virtual void mouse_goto_xy(int x, int y) override final;                \
    virtual void mouse_add_xy(int x, int y) override final;                 \
    virtual int mouse_toggle(int show) override final;
