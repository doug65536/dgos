#include "e9debug.h"
#include "debug.h"
#include "cpu/ioport.h"

#include "cpu/spinlock.h"

static spinlock_t e9debug_lock;

static int e9debug_write_debug_str(char const *str)
{
    spinlock_hold_t hold;
    int n = 0;
    hold = spinlock_lock_noirq(&e9debug_lock);
    while (*str) {
        outb(0xE9, *str++);
        ++n;
    }
    spinlock_unlock_noirq(&e9debug_lock, &hold);
    return n;
}

void e9debug_init(void)
{
    write_debug_str = e9debug_write_debug_str;
}
