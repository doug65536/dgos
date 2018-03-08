#include "e9debug.h"
#include "debug.h"
#include "cpu/ioport.h"
#include "printk.h"
#include "serial-uart.h"
#include "callout.h"
#include "string.h"
#include "cpu/spinlock.h"

#define UART_DEBUGOUT 0

static spinlock_t e9debug_lock;

#if UART_DEBUGOUT
static uart_dev_t *uart;
static bool uart_ready;

static char vt102_reset[] = "\x1B" "c";
#endif

static void e9debug_serial_ready(void*)
{
#if UART_DEBUGOUT
    uart = uart_dev_t::open(0, true);
    uart_ready = true;

    // Send some VT102 initialization sequences

    uart->write(vt102_reset, sizeof(vt102_reset)-1, sizeof(vt102_reset)-1);
#endif
}

static int e9debug_write_debug_str(char const *str, intptr_t len)
{
    int n = 0;

    if (!len)
        len = strlen(str);

    //spinlock_lock_noirq(&e9debug_lock);
    if (len > 1 && str) {
        outsb(0xE9, str, len);
    } else if (len == 1 && str) {
        outb(0xE9, str[0]);
    } else if (str) {
        while (*str) {
            outb(0xE9, *str++);
            ++n;
        }
    }

#if UART_DEBUGOUT
    if (uart_ready)
        uart->write(str, len, len);
#endif
    //spinlock_unlock_noirq(&e9debug_lock);
    return n;
}

void e9debug_init(void)
{
    write_debug_str_set_handler(e9debug_write_debug_str);
    printdbg("----------------------------------------------------\n"
             "DebugLog Started...\n"
             "----------------------------------------------------\n");
}

REGISTER_CALLOUT(e9debug_serial_ready, 0,
                 callout_type_t::constructors_ran, "000");
