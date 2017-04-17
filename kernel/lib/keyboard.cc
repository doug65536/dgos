#include "keyboard.h"
#include "printk.h"
#include "threadsync.h"
#include "cpu/control_regs.h"

struct keyboard_buffer_t {
    keyboard_event_t buffer[16];
    size_t head;
    size_t tail;

    mutex_t lock;
    condition_var_t not_empty;
};

static keyboard_buffer_t keybd_buffer;

// Drivers plug implementation into these
int (*keybd_get_modifiers)(void);
int (*keybd_set_layout_name)(char const *name);

static char const *keyboard_special_text[] = {
    // Modifier keys
    "KEYB_VK_LCTRL", "KEYB_VK_RCTRL",
    "KEYB_VK_LSHIFT", "KEYB_VK_RSHIFT",
    "KEYB_VK_LALT", "KEYB_VK_RALT",
    "KEYB_VK_LGUI", "KEYB_VK_RGUI",

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

    // Numpad digits
    "KEYB_VK_NUMPAD_0",
    "KEYB_VK_NUMPAD_1", "KEYB_VK_NUMPAD_2", "KEYB_VK_NUMPAD_3",
    "KEYB_VK_NUMPAD_4", "KEYB_VK_NUMPAD_5", "KEYB_VK_NUMPAD_6",
    "KEYB_VK_NUMPAD_7", "KEYB_VK_NUMPAD_8", "KEYB_VK_NUMPAD_9",

    // Numpad other
    "KEYB_VK_NUMPAD_DOT", "KEYB_VK_NUMPAD_ENTER",
    "KEYB_VK_NUMPAD_PLUS", "KEYB_VK_NUMPAD_MINUS",
    "KEYB_VK_NUMPAD_STAR", "KEYB_VK_NUMPAD_SLASH",

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
};

static size_t keybd_queue_next(size_t index)
{
    if (++index >= countof(keybd_buffer.buffer))
        index = 0;
    return index;
}

int keybd_event(keyboard_event_t event)
{
    mutex_lock(&keybd_buffer.lock);

    size_t next_head = keybd_queue_next(keybd_buffer.head);
    if (next_head == keybd_buffer.tail) {
        // Buffer is full
        mutex_unlock(&keybd_buffer.lock);
        return 0;
    }

    // Insert into circular buffer
    keybd_buffer.buffer[keybd_buffer.head] = event;

    // Advance head
    keybd_buffer.head = keybd_queue_next(keybd_buffer.head);

    //char const *special_text = keybd_special_text(event.vk);
    //
    //printk("%8d (%c) vk=%8x (%s)\n",
    //       event.codepoint,
    //       event.codepoint >= 0 ?
    //           event.codepoint :
    //           -event.codepoint,
    //       event.vk,
    //       special_text);

    mutex_unlock(&keybd_buffer.lock);
    condvar_wake_one(&keybd_buffer.not_empty);

    return 1;
}

keyboard_event_t keybd_waitevent(void)
{
    keyboard_event_t event;
    cpu_scoped_irq_disable intr_was_enabled;

    mutex_lock(&keybd_buffer.lock);

    while (keybd_buffer.head == keybd_buffer.tail)
        condvar_wait(&keybd_buffer.not_empty,
                     &keybd_buffer.lock);

    event = keybd_buffer.buffer[keybd_buffer.tail];

    keybd_buffer.tail = keybd_queue_next(keybd_buffer.tail);

    mutex_unlock(&keybd_buffer.lock);

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
    mutex_init(&keybd_buffer.lock);
    condvar_init(&keybd_buffer.not_empty);
}
