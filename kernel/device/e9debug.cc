#include "e9debug.h"
#include "debug.h"
#include "cpu/ioport.h"
#include "printk.h"
#include "serial-uart.h"
#include "callout.h"

#include "cpu/spinlock.h"

static spinlock_t e9debug_lock;
static uart_dev_t *uart;
static bool uart_ready;

static void e9debug_serial_ready(void*)
{
    uart = uart_dev_t::open(0, true);
    uart_ready = true;
}

static int e9debug_write_debug_str(char const *str, intptr_t len)
{
    int n = 0;
    spinlock_lock_noirq(&e9debug_lock);
    if (len && str) {
        outsb(0xE9, str, len);
    } else if (str) {
        while (*str) {
            outb(0xE9, *str++);
            ++n;
        }
    }
    if (uart_ready)
        uart->write(str, len, len);
    spinlock_unlock_noirq(&e9debug_lock);
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
