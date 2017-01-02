#include "time.h"
#include "cpu/halt.h"
#include "cpu/control_regs.h"
#include "printk.h"
#include "export.h"

// Returns number of milliseconds since system startup
static uint64_t time_ms_dummy(void);

// Returns the number of microseconds actually elapsed, if possible
// otherwise returns passed value
static uint32_t usleep_dummy(uint16_t microsec);

static uint64_t (*time_ms_vec)(void) = time_ms_dummy;
static uint32_t (*usleep_vec)(uint16_t microsec) = usleep_dummy;

// Provides a handler for time_ms until the timer is initialized
static uint64_t time_ms_dummy(void)
{
    return 0;
}

// Provides a handler for usleep until the timer is initialized
static uint32_t usleep_dummy(uint16_t microsec)
{
    return microsec;
}

void time_ms_set_handler(uint64_t (*vec)(void))
{
    time_ms_vec = vec;
}

EXPORT uint64_t time_ms(void)
{
    return time_ms_vec();
}

void usleep_set_handler(uint32_t (*vec)(uint16_t microsec))
{
    usleep_vec = vec;
}

EXPORT uint32_t usleep(uint16_t microsec)
{
    return usleep_vec(microsec);
}

EXPORT void sleep(int ms)
{
    uint64_t expiry = time_ms() + ms;

    while (time_ms() < expiry)
        halt();
}
