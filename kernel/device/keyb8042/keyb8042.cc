#include "keyb8042.h"
#include "keyboard.h"
#include "mouse.h"
#include "errno.h"

#include "irq.h"
#include "cpu/ioport.h"
#include "cpu/atomic.h"
#include "printk.h"
#include "time.h"
#include "string.h"
#include "likely.h"

__BEGIN_ANONYMOUS

// Read/write
#define KEYB_DATA   0x60

// Read only
#define KEYB_STATUS 0x64

// Write only
#define KEYB_CMD    0x64

// Status bits
#define KEYB_STAT_RDOK_BIT      0
#define KEYB_STAT_WRNOTOK_BIT   1

#define KEYB_STAT_RDOK          (1<<KEYB_STAT_RDOK_BIT)
#define KEYB_STAT_WRNOTOK       (1<<KEYB_STAT_WRNOTOK_BIT)

#define KEYB_KEY_IRQ            1
#define KEYB_MOUSE_IRQ          12

// Controller commands. These have no response unless corresponding
// REPLY are defined, or otherwise noted.

// Response is the configuration byte
#define KEYB_CMD_RDCONFIG       0x20
#define KEYB_CMD_WRCONFIG       0x60

#define KEYB_CMD_CTLTEST        0xAA
#define KEYB_REPLY_CTLTEST_OK   0x55
#define KEYB_REPLY_CTLBAD       0xFC

#define KEYB_CMD_TEST_PORT1     0xAB
#define KEYB_CMD_TEST_PORT2     0xA9
#define KEYB_CMD_TEST_PORTn_OK  0x00

#define KEYB_CMD_ENABLE_PORT1   0xAE
#define KEYB_CMD_DISABLE_PORT1  0xAD

#define KEYB_CMD_ENABLE_PORT2   0xA8
#define KEYB_CMD_DISABLE_PORT2  0xA7

#define KEYB_REPLY_ACK          0xFA
#define KEYB_REPLY_RESEND       0xFC

#define KEYB_CMD_RESET          0xFF
#define KEYB_CMD_RESEND         0xFE

#define KEYB_CMD_MOUSECMD       0xD4
#define KEYB_CMD_ENABLE_SCAN    0xF4
#define KEYB_CMD_DISABLE_SCAN   0xF5
#define KEYB_CMD_DEFAULTS       0xF6
#define KEYB_CMD_SET_SCANSET    0xF0

#define PS2MOUSE_RESET          0xFF
#define KEYB_REPLY_RESETOK      0xAA

#define PS2MOUSE_ENABLE         0xF4
#define PS2MOUSE_SETSAMPLERATE  0xF3
#define PS2MOUSE_SETRES         0xE8
#define PS2MOUSE_REPLY_ACK      0xFA
#define PS2MOUSE_REPLY_RESEND   0xFE
#define PS2MOUSE_READ_ID        0xF2

#define PS2MOUSE_RES_1CPMM      0
#define PS2MOUSE_RES_2CPMM      1
#define PS2MOUSE_RES_4CPMM      2
#define PS2MOUSE_RES_8CPMM      3

#define KEYB_CONFIG_IRQ_PORT1_BIT       0
#define KEYB_CONFIG_IRQ_PORT2_BIT       1
#define KEYB_CONFIG_POST_OK_BIT         2
#define KEYB_CONFIG_CLKDIS_PORT1_BIT    4
#define KEYB_CONFIG_CLKDIS_PORT2_BIT    5
#define KEYB_CONFIG_XLAT_PORT1_BIT      6

#define KEYB_CONFIG_IRQEN_PORT1     (1 << KEYB_CONFIG_IRQ_PORT1_BIT)
#define KEYB_CONFIG_IRQEN_PORT2     (1 << KEYB_CONFIG_IRQ_PORT2_BIT)
#define KEYB_CONFIG_POST_OK         (1 << KEYB_CONFIG_POST_OK_BIT)
#define KEYB_CONFIG_CLKDIS_PORT1    (1 << KEYB_CONFIG_CLKDIS_PORT1_BIT)
#define KEYB_CONFIG_CLKDIS_PORT2    (1 << KEYB_CONFIG_CLKDIS_PORT2_BIT)
#define KEYB_CONFIG_XLAT_PORT1      (1 << KEYB_CONFIG_XLAT_PORT1_BIT)

#define KEYB8042_DEBUG  1
#if KEYB8042_DEBUG
#define KEYB8042_TRACE(...) printk(__VA_ARGS__)
#else
#define KEYB8042_TRACE(...) ((void)0)
#endif
#define KEYB8042_MSG(...) printk("keyb8042: " __VA_ARGS__)

struct keyb8042_t : public keybd_dev_t {
public:
    void init();

private:
    enum keyb8042_key_state_t {
        NORMAL,
        IN_E0,
        IN_E1_1,
        IN_E1_2
    };

    // Keyboard state (to handle multi-byte scancodes)
    keyb8042_key_state_t state;

    // Mouse packet data
    uint64_t last_mouse_packet_time;
    size_t mouse_packet_level;
    size_t mouse_packet_size;
    size_t mouse_button_count;
    uint8_t mouse_packet[5];

    keyb8042_layout_t *layout = &keyb8042_layout_us;
    keybd_fsa_t fsa;

    // keybd_dev_t interface
    // Lookup table of keyboard layouts
    static keyb8042_layout_t *layouts[];

    static char const passthru_lookup[];

    int set_layout_name(const char *name) override final;
    int get_modifiers() override final;
    int set_indicators(int indicators) override final;

    void keyboard_handler();
    void process_mouse_packet(const uint8_t *packet);
    void mouse_handler();
    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    int send_command(uint8_t command);
    int read_data();
    int write_data(uint8_t data);
    int ctrl_command(bool has_response, int cmd, int data = -1);
    int retry_keyb_command(uint8_t command, int data = -1);
    int retry_mouse_command(uint8_t command);
    int magic_sequence(uint8_t expected_id, size_t seq_length...);
};

keyb8042_t keyb8042;

keyb8042_layout_t *keyb8042_t::layouts[] = {
    &keyb8042_layout_us,
    nullptr
};

char const keyb8042_t::passthru_lookup[] =
        " \b\n";

int keyb8042_t::set_indicators(int indicators)
{
    // FIXME:
    return -int(errno_t::ENOTSUP);
}

void keyb8042_t::keyboard_handler(void)
{
    uint8_t scancode = 0;

    scancode = inb(KEYB_DATA);

    printdbg("scancode: %#x\n", scancode);

    int32_t vk = 0;
    int sign = !!(scancode & 0x80) * -2 + 1;

    switch (keyb8042.state) {
    case NORMAL:
        if (scancode == 0xE0) {
            keyb8042.state = IN_E0;
            break;
        }
        if (scancode == 0xE1) {
            keyb8042.state = IN_E1_1;
            break;
        }
        scancode &= 0x7F;
        vk = layout->scancode[scancode];
        break;

    case IN_E0:
        scancode &= 0x7F;
        vk = layout->scancode_0xE0[scancode];
        keyb8042.state = NORMAL;
        break;

    case IN_E1_1:
        state = IN_E1_2;
        return;

    case IN_E1_2:
        state = NORMAL;
        vk = KEYB_VK_PAUSE;
        break;

    default:
        state = NORMAL;
        return;
    }

    if (vk != 0)
        fsa.deliver_vk(keybd_id, vk * sign);
}

void keyb8042_t::process_mouse_packet(uint8_t const *packet)
{
    mouse_raw_event_t event;
    event.timestamp = time_ns();
    event.hdist = 0;
    event.vdist = 0;
    event.wdist = 0;
    event.buttons = 0;

    switch (keyb8042.mouse_packet_size) {
    case 4:
        if (keyb8042.mouse_button_count == 5)
            event.buttons |= (packet[3] >> 1) & 0x18;

        // Sign extend 4 bit wheel distance
        event.wdist = (int16_t)((uint16_t)packet[3] << 12) >> 12;
    }
    // First byte:
    // +---+---+----+----+---+-----+-----+-----+
    // | ? | ? | VS | HS | 1 | MMB | RMB | LMB |
    // +---+---+----+----+---+-----+-----+-----+
    //   7   6   5    4    3    2     1     0
    //
    // Second byte is horizontal motion, positive = right
    // Third byte is vertical motion, positive = up
    //
    // Distance range -256 <= dist < 255

    // Setup sign bits
    event.hdist = ((int16_t)((uint16_t)packet[0] << 11) & 0x8000);
    event.vdist = ((int16_t)((uint16_t)packet[0] << 10) & 0x8000);

    // Sign extend
    event.hdist >>= 8;
    event.vdist >>= 8;

    event.hdist |= packet[1];
    event.vdist |= packet[2];

    event.buttons |= packet[0] & 0x07;

    mouse_event(event);
}

void keyb8042_t::mouse_handler(void)
{
    uint8_t data = inb(KEYB_DATA);

    // If it has been > 500ms since the last IRQ,
    // then discard the queued bytes (if any)
    // and start from the beginning. This handles
    // the unlikely case of getting out of sync
    // with the mouse

    uint64_t now = time_ns();
    if (last_mouse_packet_time + 500000000 < now)
        mouse_packet_level = 0;
    last_mouse_packet_time = now;

    if (keyb8042.mouse_packet_size > 0)
        mouse_packet[keyb8042.mouse_packet_level++] = data;

    if (keyb8042.mouse_packet_size &&
            keyb8042.mouse_packet_level ==
            keyb8042.mouse_packet_size) {
        process_mouse_packet(keyb8042.mouse_packet);
        mouse_packet_level = 0;
    }
}

isr_context_t *keyb8042_t::irq_handler(int irq, isr_context_t *ctx)
{
    if (irq == KEYB_KEY_IRQ)
        keyb8042.keyboard_handler();
    else if (irq == KEYB_MOUSE_IRQ)
        keyb8042.mouse_handler();

    return ctx;
}

// Prints error message and returns -1 on timeout
int keyb8042_t::send_command(uint8_t command)
{
    uint32_t max_tries = 100000;
    while ((inb(KEYB_STATUS) & KEYB_STAT_WRNOTOK) != 0 &&
           --max_tries)
        pause();

    if (max_tries > 0)
        outb(KEYB_CMD, command);

    return max_tries != 0 ? 0 : -1;
}

// Prints error message and returns -1 on timeout
int keyb8042_t::read_data(void)
{
    uint32_t max_tries = 100000;
    while ((inb(KEYB_STATUS) & KEYB_STAT_RDOK) == 0 &&
           --max_tries)
        pause();

    if (!max_tries)
        KEYB8042_MSG("Timeout reading keyboard\n");

    return max_tries ? inb(KEYB_DATA) : -1;
}

// Prints error message and returns -1 on timeout
int keyb8042_t::write_data(uint8_t data)
{
    uint32_t max_tries = 100000;
    while ((inb(KEYB_STATUS) & KEYB_STAT_WRNOTOK) != 0 &&
           --max_tries)
        pause();

    if (max_tries > 0)
        outb(KEYB_DATA, data);
    else if (!max_tries)
        KEYB8042_MSG("Timeout writing keyboard\n");

    return max_tries > 0 ? 0 : -1;
}

int keyb8042_t::ctrl_command(bool has_response, int cmd, int data)
{
    if (unlikely(send_command(cmd) < 0))
        return -1;

    if (data != -1) {
        if (write_data(data) < 0)
            return -1;
    }

    if (has_response)
        return read_data();

    return 0;
}

int keyb8042_t::retry_keyb_command(uint8_t command, int data)
{
    int last_data;

    int max_tries = 4;
    do {
        if (write_data(command) < 0)
            return -1;

        if (data != -1) {
            if (write_data(data) < 0)
                return -1;
        }

        last_data = read_data();

        if (likely(last_data == KEYB_REPLY_ACK)) {
            // Success
            return 0;
        }

        if (last_data < 0) {
            KEYB8042_MSG("Keyboard did not reply\n");
            continue;
        }

        if (last_data == KEYB_REPLY_ACK) {
            KEYB8042_MSG("Keyboard requested resend\n");
            continue;
        }
    } while (--max_tries);

    return -1;
}

int keyb8042_t::retry_mouse_command(uint8_t command)
{
    int last_data;

    int max_tries = 4;
    do {
        if (send_command(KEYB_CMD_MOUSECMD) < 0)
            return -1;

        if (write_data(command) < 0)
            return -1;

        last_data = read_data();

        if (last_data == PS2MOUSE_REPLY_ACK)
            break;

        KEYB8042_MSG("Mouse did not acknowledge\n");
    } while (--max_tries);

    return max_tries != 0 ? last_data : -1;
}

int keyb8042_t::magic_sequence(uint8_t expected_id, size_t seq_length, ...)
{
    // Send magic sequence
    va_list ap;
    va_start(ap, seq_length);
    for (size_t i = 0; i < seq_length; ++i) {
        uint8_t command = (uint8_t)va_arg(ap, unsigned);
        if (retry_mouse_command(command) < 0)
            return -1;
    }
    va_end(ap);

    // Read ID
    if (retry_mouse_command(PS2MOUSE_READ_ID) < 0)
        return -1;

    int id = read_data();

    if (id != expected_id)
        return 0;

    // Successfully detected
    return 1;
}

int keyb8042_t::set_layout_name(char const *name)
{
    for (keyb8042_layout_t **layout_srch = keyb8042.layouts;
         *layout_srch; ++layout_srch) {
        keyb8042_layout_t *this_layout = *layout_srch;

        if (!strcmp(this_layout->name, name)) {
            layout = this_layout;
            return 1;
        }
    }

    // Not found
    return 0;
}

int keyb8042_t::get_modifiers()
{
    return keyb8042.fsa.get_modifiers();
}

void keyb8042_t::init(void)
{
    // FIXME: perform USB handoff before initializing

    // FIXME: verify that ACPI bit 1 of IA PC boot architecture flags is 1

    // Disable both ports
    KEYB8042_TRACE("Disabling keyboard controller ports\n");
    send_command(KEYB_CMD_DISABLE_PORT1);
    send_command(KEYB_CMD_DISABLE_PORT2);

    // Flush incoming byte, if any
    KEYB8042_TRACE("Flushing keyboard output buffer\n");
    inb(KEYB_DATA);

    // Read config
    KEYB8042_TRACE("Reading keyboard controller config\n");
    int config = ctrl_command(true, KEYB_CMD_RDCONFIG);
    if (unlikely(config < 0)) {
        KEYB8042_MSG("Failed to read controller config\n");
        return;
    }

    KEYB8042_TRACE("Keyboard original config = %#02x\n", config);

    // Disable IRQs and translation
    config &= ~(KEYB_CONFIG_IRQEN_PORT1 |
                KEYB_CONFIG_IRQEN_PORT2 |
                KEYB_CONFIG_XLAT_PORT1);

    // Write config
    KEYB8042_TRACE("Writing keyboard controller config = %#02x\n", config);
    if (unlikely(send_command(KEYB_CMD_WRCONFIG) < 0)) {
        KEYB8042_MSG("Failed to send write config command\n");
        return;
    }
    if (unlikely(write_data(config) < 0)) {
        KEYB8042_MSG("Failed to send write config data\n");
        return;
    }

    // Detect second channel
    if (unlikely(send_command(KEYB_CMD_ENABLE_PORT2) < 0)) {
        KEYB8042_MSG("Failed to send enable port 2 command\n");
        return;
    }

    // Readback config
    int detect_config = ctrl_command(true, KEYB_CMD_RDCONFIG);
    if (unlikely(detect_config < 0)) {
        KEYB8042_MSG("Failed to read controller config detecting port 2\n");
        return;
    }

    // See if port 2 clock is disabled. If not, port 2 exists
    bool port2_exists = !(detect_config & KEYB_CONFIG_CLKDIS_PORT2);

    int ctl_test_result;
    int port1_test_result;

    ctl_test_result = ctrl_command(true, KEYB_CMD_CTLTEST);
    if (unlikely(ctl_test_result < 0)) {
        KEYB8042_MSG("Failed to start controller test\n");
        return;
    }

    if (unlikely(ctl_test_result != KEYB_REPLY_CTLTEST_OK))
        KEYB8042_MSG("Keyboard controller self test failed! result=%#02x\n",
                     ctl_test_result);

    port1_test_result = ctrl_command(true, KEYB_CMD_TEST_PORT1);
    if (unlikely(port1_test_result < 0)) {
        KEYB8042_MSG("Failed to send test port 1 command\n");
        return;
    }

    if (unlikely(port1_test_result != KEYB_CMD_TEST_PORTn_OK)) {
        KEYB8042_MSG("Keyboard port 1 self test failed! result=%#02x\n",
                     ctl_test_result);
        return;
    }

    if (port2_exists) {
        int port2_test_result;
        port2_test_result = ctrl_command(true, KEYB_CMD_TEST_PORT2);
        if (port2_test_result < 0) {
            KEYB8042_MSG("Failed to start port 2 test");
            return;
        }

        if (port2_test_result != KEYB_CMD_TEST_PORTn_OK) {
            KEYB8042_MSG("Keyboard port 2 self test failed! result=%#02x\n",
                         port2_test_result);
            port2_exists = 0;
        }
    }

    // Reset
    KEYB8042_TRACE("Resetting keyboard\n");
    if (unlikely(retry_keyb_command(KEYB_CMD_RESET) < 0)) {
        KEYB8042_MSG("Failed to send keyboard reset command\n");
        return;
    }

    // Read reset result
    port1_test_result = read_data();
    if (unlikely(port1_test_result != KEYB_REPLY_RESETOK)) {
        KEYB8042_TRACE("Keyboard reset failed, data=%#x\n", port1_test_result);
        return;
    }

    KEYB8042_TRACE("Enabling keyboard port\n");
    if (send_command(KEYB_CMD_ENABLE_PORT1) < 0)
        return;

    if (port2_exists) {
        KEYB8042_TRACE("Enabling mouse port\n");
        if (send_command(KEYB_CMD_ENABLE_PORT2) < 0)
            return;
    }

    config &= ~KEYB_CONFIG_CLKDIS_PORT1;
    config &= ~KEYB_CONFIG_XLAT_PORT1;
    config |= KEYB_CONFIG_IRQEN_PORT1;
    //config |= KEYB_CONFIG_XLAT_PORT1;
    if (port2_exists) {
        config &= ~KEYB_CONFIG_CLKDIS_PORT2;
        config |= KEYB_CONFIG_IRQEN_PORT2;
    }

    KEYB8042_TRACE("Writing keyboard controller config = %#02x\n", config);
    if (send_command(KEYB_CMD_WRCONFIG) < 0)
        return;
    if (write_data(config) < 0)
        return;

    // Choose scanset 1
    if (retry_keyb_command(KEYB_CMD_SET_SCANSET, 1) < 0) {
        KEYB8042_MSG("Failed to send set-scanset keyboard command\n");
        return;
    }

    if (retry_keyb_command(KEYB_CMD_ENABLE_SCAN) < 0) {
        KEYB8042_MSG("Failed to enable keyboard scanning\n");
        return;
    }

    // Reset mouse
    KEYB8042_TRACE("Resetting mouse\n");
    if (retry_mouse_command(PS2MOUSE_RESET) < 0)
        return;
    KEYB8042_TRACE("Reading reset ok\n");
    if (unlikely(read_data() != KEYB_REPLY_RESETOK))
        KEYB8042_MSG("Mouse did not acknowledge\n");
    KEYB8042_TRACE("Reading reset result\n");
    if (read_data() != 0x00)
        KEYB8042_MSG("Mouse did not acknowledge\n");

    // Set mouse resolution
    KEYB8042_TRACE("Setting mouse resolution\n");
    if (retry_mouse_command(PS2MOUSE_SETRES) < 0)
        return;
    if (retry_mouse_command(PS2MOUSE_RES_1CPMM) < 0)
        return;

    // Enable mouse stream mode
    KEYB8042_TRACE("Setting mouse to stream mode\n");
    if (retry_mouse_command(PS2MOUSE_ENABLE) < 0)
        return;

    keyb8042.mouse_packet_size = 3;
    keyb8042.mouse_button_count = 2;

    // Attempt to detect better mouse

    if (magic_sequence(
                0x04, 6, 0xF3, 0xC8, 0xF3, 0xC8, 0xF3, 0x50) > 0) {
        // Attempt to detect 5 button mouse with wheel
        // (Intellimouse Explorer compatible)
        printk("Detected 5 button mouse with wheel\n");
        keyb8042.mouse_button_count = 5;
        keyb8042.mouse_packet_size = 4;
    } else if (magic_sequence(
                   0x03, 6, 0xF3, 0xC8, 0xF3, 0x64, 0xF3, 0x50) > 0) {
        // Attempt to detect 3 button mouse with wheel
        // (Intellimouse compatible)
        printk("Detected mouse with wheel\n");
        keyb8042.mouse_button_count = 3;
        keyb8042.mouse_packet_size = 4;
    }

    // Set mouse sampling rate
    KEYB8042_TRACE("Setting mouse sampling rate\n");
    if (retry_mouse_command(PS2MOUSE_SETSAMPLERATE) < 0)
        return;
    if (retry_mouse_command(100) < 0)
        return;

    printk("Keyboard/mouse initialization complete\n");

    keybd_add(this);

    irq_hook(1, irq_handler, "keyb8042");
    irq_setmask(1, 1);

    if (port2_exists) {
        printk("Mouse enabled\n");
        mouse_file_init();
        irq_hook(12, irq_handler, "keyb8042_mouse");
        irq_setmask(12, 1);
    }

    printk("Keyboard enabled\n");
}

int module_main(int argc, char const * const * argv)
{
    keyb8042.init();
    return 0;
}

__END_ANONYMOUS
