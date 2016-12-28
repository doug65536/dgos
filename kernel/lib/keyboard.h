#pragma once
#include "types.h"

// Special keys are encoded as codepoints beyond
// the unicode range
typedef enum keyboard_virtual_key_t {
    KEYB_VK_BASE = 0x120000,

    // Modifier keys
    KEYB_VK_LCTRL, KEYB_VK_RCTRL,
    KEYB_VK_LSHIFT, KEYB_VK_RSHIFT,
    KEYB_VK_LALT, KEYB_VK_RALT,
    KEYB_VK_LGUI, KEYB_VK_RGUI,

    // Lock keys
    KEYB_VK_CAPSLOCK, KEYB_VK_NUMLOCK, KEYB_VK_SCRLOCK,

    // Function keys
    KEYB_VK_F1, KEYB_VK_F2, KEYB_VK_F3, KEYB_VK_F4,
    KEYB_VK_F5, KEYB_VK_F6, KEYB_VK_F7, KEYB_VK_F8,
    KEYB_VK_F9, KEYB_VK_F10, KEYB_VK_F11, KEYB_VK_F12,

    // Extended function keys
    KEYB_VK_F13, KEYB_VK_F14, KEYB_VK_F15, KEYB_VK_F16,
    KEYB_VK_F17, KEYB_VK_F18, KEYB_VK_F19, KEYB_VK_F20,
    KEYB_VK_F21, KEYB_VK_F22, KEYB_VK_F23, KEYB_VK_F24,

    // Menu keys
    KEYB_VK_LMENU, KEYB_VK_RMENU,

    // Numpad digits
    KEYB_VK_NUMPAD_0,
    KEYB_VK_NUMPAD_1, KEYB_VK_NUMPAD_2, KEYB_VK_NUMPAD_3,
    KEYB_VK_NUMPAD_4, KEYB_VK_NUMPAD_5, KEYB_VK_NUMPAD_6,
    KEYB_VK_NUMPAD_7, KEYB_VK_NUMPAD_8, KEYB_VK_NUMPAD_9,

    // Numpad other
    KEYB_VK_NUMPAD_DOT, KEYB_VK_NUMPAD_ENTER,
    KEYB_VK_NUMPAD_PLUS, KEYB_VK_NUMPAD_MINUS,
    KEYB_VK_NUMPAD_STAR, KEYB_VK_NUMPAD_SLASH,

    // Editing keys
    KEYB_VK_INS, KEYB_VK_DEL,

    // Movement keys
    KEYB_VK_HOME, KEYB_VK_END,
    KEYB_VK_PGUP, KEYB_VK_PGDN,
    KEYB_VK_UP, KEYB_VK_DOWN, KEYB_VK_LEFT, KEYB_VK_RIGHT,

    // Special keys
    KEYB_VK_PRNSCR, KEYB_VK_PAUSE, KEYB_VK_SYSRQ,

    // Power keys
    KEYB_VK_POWER, KEYB_VK_SLEEP, KEYB_VK_WAKE,

    // Shortcut keys
    KEYB_VK_BROWSER_HOME, KEYB_VK_BACK, KEYB_VK_FORWARD,

    // Volume keys
    KEYB_VK_VOL_MUTE, KEYB_VK_VOL_DOWN, KEYB_VK_VOL_UP,

    // Media keys
    KEYB_VK_MEDIA_STOP, KEYB_VK_MEDIA_PAUSE,
    KEYB_VK_MEDIA_PAUSE_PLAY, KEYB_VK_MEDIA_PLAY,
    KEYB_VK_MEDIA_PREVIOUS, KEYB_VK_MEDIA_NEXT,

    // Display keys
    KEYB_VK_BRIGHTNESS_DOWN, KEYB_VK_BRIGHTNESS_UP,
    KEYB_VK_CONTRAST_DOWN, KEYB_VK_CONTRAST_UP,

    // Laptop keys
    KEYB_VK_WIRELESS_TOGGLE, KEYB_VK_EXT_DISPLAY_TOGGLE,
    KEYB_VK_TOUCHPAD_TOGGLE, KEYB_VK_KEYLIGHT_TOGGLE,

    //
    // Ranges

    // Function key range
    KEYB_VK_FN_ST = KEYB_VK_F1,
    KEYB_VK_FN_EN = KEYB_VK_F24,

    // Numpad range
    KEYB_VK_NUMPAD_ST = KEYB_VK_NUMPAD_0,
    KEYB_VK_NUMPAD_EN = KEYB_VK_NUMPAD_SLASH
} keyboard_virtual_key_t;

typedef struct keyboard_event_t {
    // Positive values indicate keydown/repeat
    // Negative values indicate keyup
    int vk;
    int codepoint;

    int flags;
} keyboard_event_t;

// Mouse events use these too:

#define KEYMODIFIER_FLAG_SHIFT_BIT     0
#define KEYMODIFIER_FLAG_CTRL_BIT      2
#define KEYMODIFIER_FLAG_ALT_BIT       4
#define KEYMODIFIER_FLAG_GUI_BIT       6

#define KEYMODIFIER_FLAG_SHIFT         (1 << KEYMODIFIER_FLAG_SHIFT_BIT)
#define KEYMODIFIER_FLAG_CTRL          (1 << KEYMODIFIER_FLAG_CTRL_BIT)
#define KEYMODIFIER_FLAG_ALT           (1 << KEYMODIFIER_FLAG_ALT_BIT)
#define KEYMODIFIER_FLAG_GUI           (1 << KEYMODIFIER_FLAG_GUI_BIT)

// Get the human readable text for a special codepoint
char const *keybd_special_text(int codepoint);

// Called from drivers to add a keyboard event
int keybd_event(keyboard_event_t event);

// Called to read the keyboard queue
keyboard_event_t keybd_waitevent(void);

// Plugged in by drivers
extern int (*keybd_set_layout_name)(char const *name);
extern int (*keybd_get_modifiers)(void);

void keybd_init(void);
