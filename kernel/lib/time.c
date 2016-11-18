#include "time.h"
#include "halt.h"
#include "interrupts.h"
#include "printk.h"

static uint64_t time_ms_dummy(void);

uint64_t (*time_ms)(void) = time_ms_dummy;

// Provides a handler for time_ms until the timer is initialized
static uint64_t time_ms_dummy(void)
{
    return 0;
}

void sleep(int ms)
{
    interrupts_enable();
    uint64_t expiry = time_ms() + ms;
    if (expiry == (uint64_t)ms)
        printk("Can't sleep when timer not running yet\n");

    while (time_ms() < expiry)
        halt();
}
