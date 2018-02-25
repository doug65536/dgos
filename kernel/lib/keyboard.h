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
    KEYB_VK_NUMPAD_EQUALS, KEYB_VK_NUMPAD_COMMA,
    KEYB_VK_NUMPAD_ALT_ERASE,

    // Numpad Hex
    KEYB_VK_NUMPAD_A, KEYB_VK_NUMPAD_B, KEYB_VK_NUMPAD_C,
    KEYB_VK_NUMPAD_D, KEYB_VK_NUMPAD_E, KEYB_VK_NUMPAD_F,

    // Numpad mem
    KEYB_VK_NUMPAD_MEM_STORE,
    KEYB_VK_NUMPAD_MEM_RECALL,
    KEYB_VK_NUMPAD_MEM_ADD,
    KEYB_VK_NUMPAD_MEM_SUB,
    KEYB_VK_NUMPAD_MEM_MUL,
    KEYB_VK_NUMPAD_MEM_DIV,

    // Numpad parentheses/braces
    KEYB_VK_NUMPAD_PAREN_OPEN, KEYB_VK_NUMPAD_PAREN_CLOSE,
    KEYB_VK_NUMPAD_BRACE_OPEN, KEYB_VK_NUMPAD_BRACE_CLOSE,

    // Numpad number base
    KEYB_VK_NUMPAD_BINARY, KEYB_VK_NUMPAD_OCTAL,
    KEYB_VK_NUMPAD_DECIMAL, KEYB_VK_NUMPAD_HEX,

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

    // Obscure keys
    KEYB_VK_HELP, KEYB_VK_MENU, KEYB_VK_SELECT, KEYB_VK_STOP,
    KEYB_VK_AGAIN, KEYB_VK_UNDO, KEYB_VK_FIND,
    KEYB_VK_OUT, KEYB_VK_OPER, KEYB_VK_CLEAR, KEYB_VK_CRSEL, KEYB_VK_EXSEL,
    KEYB_VK_SEPARATOR, KEYB_VK_CANCEL, KEYB_VK_PRIOR, KEYB_VK_EXECUTE,

    // Separators
    KEYB_VK_THOUSANDS_SEP, KEYB_VK_DECIMAL_SEP,

    // Currency
    KEYB_VK_CURRENCY_UNIT, KEYB_VK_CURRENCY_SUBUNIT,

    // Multi-key
    KEYB_VK_00, KEYB_VK_000,

    // Locking modifiers
    KEYB_VK_LOCKING_CAPS, KEYB_VK_LOCKING_NUM, KEYB_VK_LOCKING_SCROLL,

    // Clipboard keys
    KEYB_VK_CUT, KEYB_VK_COPY, KEYB_VK_PASTE,

    // International
    KEYB_VK_INTL_1, KEYB_VK_INTL_2, KEYB_VK_INTL_3,
    KEYB_VK_INTL_4, KEYB_VK_INTL_5, KEYB_VK_INTL_6,
    KEYB_VK_INTL_7, KEYB_VK_INTL_8, KEYB_VK_INTL_9,

    // Lang
    KEYB_VK_LANG_1, KEYB_VK_LANG_2, KEYB_VK_LANG_3,
    KEYB_VK_LANG_4, KEYB_VK_LANG_5, KEYB_VK_LANG_6,
    KEYB_VK_LANG_7, KEYB_VK_LANG_8, KEYB_VK_LANG_9,

    KEYB_VK_NUMPAD_TAB,
    KEYB_VK_NUMPAD_BACKSPACE,

    KEYB_VK_NUMPAD_XOR, KEYB_VK_NUMPAD_CARET, KEYB_VK_NUMPAD_PERCENT,
    KEYB_VK_NUMPAD_LESSTHAN, KEYB_VK_NUMPAD_GREATERTHAN,
    KEYB_VK_NUMPAD_AMPERSAND, KEYB_VK_NUMPAD_AMPERSAND2,
    KEYB_VK_NUMPAD_PIPE, KEYB_VK_NUMPAD_PIPE2,
    KEYB_VK_NUMPAD_COLON, KEYB_VK_NUMPAD_HASH,
    KEYB_VK_NUMPAD_SPACE, KEYB_VK_NUMPAD_AT,
    KEYB_VK_NUMPAD_EXCLAMATION, KEYB_VK_NUMPAD_SIGN,
    KEYB_VK_NUMPAD_CLEAR, KEYB_VK_NUMPAD_CLEAR_ENTRY,

    //
    // Ranges

    // Function key range
    KEYB_VK_FN_ST = KEYB_VK_F1,
    KEYB_VK_FN_EN = KEYB_VK_F24,

    // Numpad range
    KEYB_VK_NUMPAD_ST = KEYB_VK_NUMPAD_0,
    KEYB_VK_NUMPAD_EN = KEYB_VK_NUMPAD_SLASH
} keyboard_virtual_key_t;

struct keyboard_event_t {
    // Positive values indicate keydown/repeat
    // Negative values indicate keyup
    int vk;
    int codepoint;

    int flags;
};

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
