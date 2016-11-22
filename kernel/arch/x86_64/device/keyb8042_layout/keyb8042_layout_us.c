#include "keyboard.h"
#include "device/keyb8042.h"

// Scancodes resolve to an ASCII equivalent, or,
// a special code >= SPECIAL_BASE
// These are all of the keys that existed on the original XT keyboard
static int const keyb8042_scancode_us[128] = {
    // 0x00
    0, '\x1b',

    // 0x02
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',

    // 0x0F
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n',

    // 0x1D
    KEYB_VK_LCTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',

    // 0x2A
    KEYB_VK_LSHIFT, '\\',

    // 0x2C
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/',

    // 0x36
    KEYB_VK_RSHIFT,

    // 0x37
    KEYB_VK_NUMPAD_STAR,

    // 0x38
    KEYB_VK_LALT, ' ',

    // 0x3A
    KEYB_VK_CAPSLOCK,

    // 0x3B
    KEYB_VK_F1, KEYB_VK_F2, KEYB_VK_F3, KEYB_VK_F4,

    // 0x3F
    KEYB_VK_F5, KEYB_VK_F6, KEYB_VK_F7, KEYB_VK_F8,

    // 0x43
    KEYB_VK_F9, KEYB_VK_F10,

    // 0x45
    KEYB_VK_NUMLOCK, KEYB_VK_SCRLOCK,

    // 0x47
    KEYB_VK_NUMPAD_7, KEYB_VK_NUMPAD_8, KEYB_VK_NUMPAD_9, KEYB_VK_NUMPAD_MINUS,

    // 0x4B
    KEYB_VK_NUMPAD_4, KEYB_VK_NUMPAD_5, KEYB_VK_NUMPAD_6, KEYB_VK_NUMPAD_PLUS,

    // 0x4F
    KEYB_VK_NUMPAD_1, KEYB_VK_NUMPAD_2, KEYB_VK_NUMPAD_3,

    // 0x52
    KEYB_VK_NUMPAD_0, KEYB_VK_NUMPAD_DOT,

    // 0x54
    KEYB_VK_SYSRQ, 0, '\\',

    // 0x57
    KEYB_VK_F11, KEYB_VK_F12
};

static int const keyb8042_scancode_us_0xE0[128] = {
    // 0x00
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // 0x10
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // 0x1C
    KEYB_VK_NUMPAD_ENTER, KEYB_VK_RCTRL, 0, 0,

    // 0x20
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // 0x30
    0, 0, 0, 0, 0, KEYB_VK_NUMPAD_SLASH, 0, KEYB_VK_PRNSCR,

    // 0x38
    KEYB_VK_RALT, 0, 0, 0, 0, 0, 0, 0,

    // 0x40
    0, 0, 0, 0, 0, KEYB_VK_NUMLOCK, 0, KEYB_VK_HOME,

    // 0x48
    KEYB_VK_UP, KEYB_VK_PGUP, 0,

    // 0x4B
    KEYB_VK_LEFT, 0,

    // 0x4D
    KEYB_VK_RIGHT, 0, KEYB_VK_END,

    // 0x50
    KEYB_VK_DOWN, KEYB_VK_PGDN, KEYB_VK_INS, KEYB_VK_DEL, 0, 0, 0, 0,

    // 0x58
    0, 0, 0, KEYB_VK_LGUI, KEYB_VK_RGUI, KEYB_VK_RMENU,

    // 0x5E
    KEYB_VK_POWER, KEYB_VK_SLEEP,

    // 0x60
    0, 0, 0, KEYB_VK_WAKE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // 0x70
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// The shifted character is stored immediately after
// its corresponding unshifted character
static char const keyb8042_shifted_lookup_us[] =
        "`~1!2@3#4$5%6^7&8*9(0)-_=+[{]};:'\"\\|,<.>/?";

keyb8042_layout_t keyb8042_layout_us = {
    "us",
    keyb8042_scancode_us,
    keyb8042_scancode_us_0xE0,
    keyb8042_shifted_lookup_us
};
