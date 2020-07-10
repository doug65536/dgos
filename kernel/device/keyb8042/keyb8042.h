#pragma once

struct keyb8042_layout_t {
    char const * const name;
    int const * const scancode;
    int const * const scancode_0xE0;
    char const * const shifted_lookup;
};

// Layouts
extern keyb8042_layout_t keyb8042_layout_us;

void keyb8042_init(void);
