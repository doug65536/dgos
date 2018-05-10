#include "keyboard.h"
#include "mutex.h"
#include "printk.h"

#define DEBUG_KEYBD 1
#if DEBUG_KEYBD
#define KEYBD_TRACE(...) printdbg("keybd: " __VA_ARGS__)
#else
#define KEYBD_TRACE(...) ((void)0)
#endif

struct keyboard_buffer_t {
    keyboard_event_t buffer[16];
    size_t head;
    size_t tail;

    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;

    lock_type lock;
    condition_variable not_empty;
};

static keyboard_buffer_t keybd_buffer;

// Drivers plug implementation into these
int (*keybd_get_modifiers)(void);
int (*keybd_set_layout_name)(char const *name);

// The order here must match the order of KEYB_VK_NUMPAD_* enum
char const keybd_fsa_t::numpad_ascii[] =
        "0123456789ABCDEF+-*/=,.\n\t\b^%:# @!&|<>(){}";

char const keybd_fsa_t::passthru_lookup[] =
    " \b\n";

char const keybd_fsa_t::shifted_lookup_us[] =
        "`~1!2@3#4$5%6^7&8*9(0)-_=+[{]};:'\"\\|,<.>/?";

static char const *keyboard_special_text[] = {
    // Modifier keys
    "KEYB_VK_LCTRL", "KEYB_VK_LSHIFT", "KEYB_VK_LALT", "KEYB_VK_LGUI",
    "KEYB_VK_RCTRL", "KEYB_VK_RSHIFT", "KEYB_VK_RALT", "KEYB_VK_RGUI",

    // Lock keys
    "KEYB_VK_CAPSLOCK", "KEYB_VK_NUMLOCK", "KEYB_VK_SCRLOCK",

    // Function keys
    "KEYB_VK_F1", "KEYB_VK_F2", "KEYB_VK_F3", "KEYB_VK_F4",
    "KEYB_VK_F5", "KEYB_VK_F6", "KEYB_VK_F7", "KEYB_VK_F8",
    "KEYB_VK_F9", "KEYB_VK_F10", "KEYB_VK_F11", "KEYB_VK_F12",

    // Extended function keys
    "KEYB_VK_F13", "KEYB_VK_F14", "KEYB_VK_F15", "KEYB_VK_F16",
    "KEYB_VK_F17", "KEYB_VK_F18", "KEYB_VK_F19", "KEYB_VK_F20",
    "KEYB_VK_F21", "KEYB_VK_F22", "KEYB_VK_F23", "KEYB_VK_F24",

    // Menu keys
    "KEYB_VK_LMENU", "KEYB_VK_RMENU",

    //
    // Printable numpad keys

    // 0123456789ABCDEF+-*/=,.\n\t\b^# @!&|<>(){}

    // Numpad digits
    "KEYB_VK_NUMPAD_0",
    "KEYB_VK_NUMPAD_1", "KEYB_VK_NUMPAD_2", "KEYB_VK_NUMPAD_3",
    "KEYB_VK_NUMPAD_4", "KEYB_VK_NUMPAD_5", "KEYB_VK_NUMPAD_6",
    "KEYB_VK_NUMPAD_7", "KEYB_VK_NUMPAD_8", "KEYB_VK_NUMPAD_9",

    // Numpad Hex
    "KEYB_VK_NUMPAD_A", "KEYB_VK_NUMPAD_B", "KEYB_VK_NUMPAD_C",
    "KEYB_VK_NUMPAD_D", "KEYB_VK_NUMPAD_E", "KEYB_VK_NUMPAD_F",

    // Numpad printable
    "KEYB_VK_NUMPAD_PLUS", "KEYB_VK_NUMPAD_MINUS",
    "KEYB_VK_NUMPAD_STAR", "KEYB_VK_NUMPAD_SLASH",
    "KEYB_VK_NUMPAD_EQUALS", "KEYB_VK_NUMPAD_COMMA",
    "KEYB_VK_NUMPAD_DOT", "KEYB_VK_NUMPAD_ENTER",
    "KEYB_VK_NUMPAD_TAB", "KEYB_VK_NUMPAD_BACKSPACE",
    "KEYB_VK_NUMPAD_CARET", "KEYB_VK_NUMPAD_PERCENT",
    "KEYB_VK_NUMPAD_COLON", "KEYB_VK_NUMPAD_HASH", "KEYB_VK_NUMPAD_SPACE",
    "KEYB_VK_NUMPAD_AT", "KEYB_VK_NUMPAD_EXCLAMATION",
    "KEYB_VK_NUMPAD_AMPERSAND",  "KEYB_VK_NUMPAD_PIPE",

    // Numpad relational/parentheses/braces
    "KEYB_VK_NUMPAD_LESSTHAN", "KEYB_VK_NUMPAD_GREATERTHAN",
    "KEYB_VK_NUMPAD_PAREN_OPEN", "KEYB_VK_NUMPAD_PAREN_CLOSE",
    "KEYB_VK_NUMPAD_BRACE_OPEN", "KEYB_VK_NUMPAD_BRACE_CLOSE",

    // Numpad number base
    "KEYB_VK_NUMPAD_BINARY", "KEYB_VK_NUMPAD_OCTAL",
    "KEYB_VK_NUMPAD_DECIMAL", "KEYB_VK_NUMPAD_HEX",

    "KEYB_VK_NUMPAD_XOR",

    // Numpad mem
    "KEYB_VK_NUMPAD_MEM_STORE", "KEYB_VK_NUMPAD_MEM_RECALL",
    "KEYB_VK_NUMPAD_MEM_ADD", "KEYB_VK_NUMPAD_MEM_SUB",
    "KEYB_VK_NUMPAD_MEM_MUL", "KEYB_VK_NUMPAD_MEM_DIV",

    // Numpad other
    "KEYB_VK_NUMPAD_ALT_ERASE",
    "KEYB_VK_NUMPAD_SIGN",
    "KEYB_VK_NUMPAD_CLEAR", "KEYB_VK_NUMPAD_CLEAR_ENTRY",

    // Numpad multi character
    "KEYB_VK_NUMPAD_AMPERSAND2", "KEYB_VK_NUMPAD_PIPE2",

    // Editing keys
    "KEYB_VK_INS", "KEYB_VK_DEL",

    // Movement keys
    "KEYB_VK_HOME", "KEYB_VK_END",
    "KEYB_VK_PGUP", "KEYB_VK_PGDN",
    "KEYB_VK_UP", "KEYB_VK_DOWN", "KEYB_VK_LEFT", "KEYB_VK_RIGHT",

    // Special keys
    "KEYB_VK_PRNSCR", "KEYB_VK_PAUSE", "KEYB_VK_SYSRQ",

    // Power keys
    "KEYB_VK_POWER", "KEYB_VK_SLEEP", "KEYB_VK_WAKE",

    // Shortcut keys
    "KEYB_VK_BROWSER_HOME", "KEYB_VK_BACK", "KEYB_VK_FORWARD",

    // Volume keys
    "KEYB_VK_VOL_MUTE", "KEYB_VK_VOL_DOWN", "KEYB_VK_VOL_UP",

    // Media keys
    "KEYB_VK_MEDIA_STOP", "KEYB_VK_MEDIA_PAUSE",
    "KEYB_VK_MEDIA_PAUSE_PLAY", "KEYB_VK_MEDIA_PLAY",
    "KEYB_VK_MEDIA_PREVIOUS", "KEYB_VK_MEDIA_NEXT",

    // Display keys
    "KEYB_VK_BRIGHTNESS_DOWN", "KEYB_VK_BRIGHTNESS_UP",
    "KEYB_VK_CONTRAST_DOWN", "KEYB_VK_CONTRAST_UP",

    // Laptop keys
    "KEYB_VK_WIRELESS_TOGGLE", "KEYB_VK_EXT_DISPLAY_TOGGLE",
    "KEYB_VK_TOUCHPAD_TOGGLE", "KEYB_VK_KEYLIGHT_TOGGLE",

    // Obscure keys
    "KEYB_VK_HELP", "KEYB_VK_MENU", "KEYB_VK_SELECT", "KEYB_VK_STOP",
    "KEYB_VK_AGAIN", "KEYB_VK_UNDO", "KEYB_VK_FIND",
    "KEYB_VK_OUT", "KEYB_VK_OPER", "KEYB_VK_CLEAR", "KEYB_VK_CRSEL",
    "KEYB_VK_EXSEL", "KEYB_VK_SEPARATOR", "KEYB_VK_CANCEL", "KEYB_VK_PRIOR",
    "KEYB_VK_EXECUTE",

    // Separators
    "KEYB_VK_THOUSANDS_SEP", "KEYB_VK_DECIMAL_SEP",

    // Currency
    "KEYB_VK_CURRENCY_UNIT", "KEYB_VK_CURRENCY_SUBUNIT",

    // Multi-key
    "KEYB_VK_00", "KEYB_VK_000",

    // Locking modifiers
    "KEYB_VK_LOCKING_CAPS", "KEYB_VK_LOCKING_NUM", "KEYB_VK_LOCKING_SCROLL",

    // Clipboard keys
    "KEYB_VK_CUT", "KEYB_VK_COPY", "KEYB_VK_PASTE",

    // International
    "KEYB_VK_INTL_1", "KEYB_VK_INTL_2", "KEYB_VK_INTL_3",
    "KEYB_VK_INTL_4", "KEYB_VK_INTL_5", "KEYB_VK_INTL_6",
    "KEYB_VK_INTL_7", "KEYB_VK_INTL_8", "KEYB_VK_INTL_9",

    // Lang
    "KEYB_VK_LANG_1", "KEYB_VK_LANG_2", "KEYB_VK_LANG_3",
    "KEYB_VK_LANG_4", "KEYB_VK_LANG_5", "KEYB_VK_LANG_6",
    "KEYB_VK_LANG_7", "KEYB_VK_LANG_8", "KEYB_VK_LANG_9",
};

static size_t keybd_queue_next(size_t index)
{
    if (++index >= countof(keybd_buffer.buffer))
        index = 0;
    return index;
}

int keybd_event(keyboard_event_t event)
{
    keyboard_buffer_t::scoped_lock buffer_lock(keybd_buffer.lock);

    size_t next_head = keybd_queue_next(keybd_buffer.head);
    if (next_head == keybd_buffer.tail) {
        // Buffer is full
        return 0;
    }

    KEYBD_TRACE("event codepoint=%c (%d), vk=%s (%d)\n",
                event.codepoint >= ' ' && event.codepoint < 126
                ? event.codepoint : '.', event.codepoint,
                keybd_special_text(event.vk), event.vk);

    // Insert into circular buffer
    keybd_buffer.buffer[keybd_buffer.head] = event;

    // Advance head
    keybd_buffer.head = keybd_queue_next(keybd_buffer.head);

    keybd_buffer.not_empty.notify_all();

    return 1;
}

keyboard_event_t keybd_waitevent(void)
{
    keyboard_event_t event;

    keyboard_buffer_t::scoped_lock buffer_lock(keybd_buffer.lock);

    while (keybd_buffer.head == keybd_buffer.tail)
        keybd_buffer.not_empty.wait(buffer_lock);

    event = keybd_buffer.buffer[keybd_buffer.tail];

    keybd_buffer.tail = keybd_queue_next(keybd_buffer.tail);

    return event;
}

char const *keybd_special_text(int codepoint)
{
    if (codepoint < 0)
        codepoint = -codepoint;

    int index = codepoint - KEYB_VK_BASE - 1;
    if (index >= 0 && index < (int)countof(keyboard_special_text)) {
        return keyboard_special_text[index];
    } else if (codepoint < 0x101000)
        return "";
    return 0;
}

void keybd_init(void)
{
}

keybd_fsa_t::keybd_fsa_t()
    : shifted_lookup(shifted_lookup_us)
    , alt_code(0)
    , shift_state(0)
{
}

void keybd_fsa_t::deliver_vk(int vk)
{
    int is_keyup = vk < 0;

    // Absolute
    vk = (vk ^ -is_keyup) - (-is_keyup);

    if (vk > KEYB_VK_BASE) {
        int shift_mask;

        // Update shift state
        switch (vk) {
        default: shift_mask = 0; break;
        case KEYB_VK_LCTRL: shift_mask = KEYB_LCTRL_DOWN; break;
        case KEYB_VK_LSHIFT: shift_mask = KEYB_LSHIFT_DOWN; break;
        case KEYB_VK_LALT: shift_mask = KEYB_LALT_DOWN; break;
        case KEYB_VK_LGUI: shift_mask = KEYB_LGUI_DOWN; break;
        case KEYB_VK_RCTRL: shift_mask = KEYB_RCTRL_DOWN; break;
        case KEYB_VK_RSHIFT: shift_mask = KEYB_RSHIFT_DOWN; break;
        case KEYB_VK_RALT: shift_mask = KEYB_RALT_DOWN; break;
        case KEYB_VK_RGUI: shift_mask = KEYB_RGUI_DOWN; break;
        }

        // Update shift state bit
        int up_mask = -is_keyup;
        int down_mask = ~up_mask;
        shift_state |= (shift_mask & down_mask);
        shift_state &= ~(shift_mask & up_mask);
    }

    //
    // Determine ASCII code

    int codepoint = 0;
    if (vk >= 'A' && vk <= 'Z') {
        if (shift_state & KEYB_ALT_DOWN) {
            // No ascii
        } else if (shift_state & KEYB_SHIFT_DOWN) {
            codepoint = vk;
        } else {
            // Not shifted
            codepoint = vk - 'A' + 'a';
        }
    } else if (vk >= KEYB_VK_NUMPAD_0 &&
               vk < KEYB_VK_NUMPAD_ASCIIEND) {
        if (shift_state & KEYB_ALT_DOWN &&
				vk >= KEYB_VK_NUMPAD_0 && vk <= KEYB_VK_NUMPAD_9) {
            if (is_keyup) {
                // Add decimal digit to alt code
                alt_code = alt_code * 10 + vk - KEYB_VK_NUMPAD_0;
            }
        } else {
            codepoint = numpad_ascii[vk - KEYB_VK_NUMPAD_ST];
        }
    } else if (strchr(passthru_lookup, vk)) {
        codepoint = vk;
    } else if (vk < 0x100) {
        char const *lookup = strchr(shifted_lookup, vk);
        if (lookup) {
            codepoint = (shift_state & KEYB_SHIFT_DOWN)
                    ? lookup[1]
                    : lookup[0];
        }
    }

    if (shift_state & KEYB_CTRL_DOWN)
        codepoint &= 0x1F;

    keyboard_event_t event;

    event.flags = get_modifiers();

    if (vk || codepoint) {
        event.codepoint = is_keyup ? -codepoint : codepoint;
        event.vk = is_keyup ? -vk : vk;
        keybd_event(event);
    }

    if (is_keyup && (vk == KEYB_VK_LALT || vk == KEYB_VK_RALT)) {
        if (alt_code != 0) {
            if (alt_code < 0x101000) {
                event.vk = 0;

                // Generate keydown and keyup event for entered codepoint

                event.codepoint = alt_code;
                keybd_event(event);

                event.codepoint = -alt_code;
                keybd_event(event);
            }

            alt_code = 0;
        }
    }
}

int keybd_fsa_t::get_modifiers()
{
    int flags = 0;

    if (shift_state & KEYB_SHIFT_DOWN)
        flags |= KEYMODIFIER_FLAG_SHIFT;

    if (shift_state & KEYB_CTRL_DOWN)
        flags |= KEYMODIFIER_FLAG_CTRL;

    if (shift_state & KEYB_ALT_DOWN)
        flags |= KEYMODIFIER_FLAG_ALT;

    if (shift_state & KEYB_GUI_DOWN)
        flags |= KEYMODIFIER_FLAG_GUI;

    return flags;
}
