#include "time.h"
#include "cpu/halt.h"
#include "cpu/control_regs.h"
#include "thread.h"
#include "printk.h"
#include "export.h"

// Returns number of milliseconds since system startup
static uint64_t time_ms_dummy(void);

// Returns the number of microseconds actually elapsed, if possible
// otherwise returns passed value
static uint32_t usleep_dummy(uint16_t microsec);

static uint64_t (*time_ms_vec)(void) = time_ms_dummy;
static uint32_t (*usleep_vec)(uint16_t microsec) = usleep_dummy;

static time_ofday_handler_t time_gettimeofday_vec;

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
    thread_sleep_for(ms);
}

void time_ofday_set_handler(time_ofday_handler_t handler)
{
    time_gettimeofday_vec = handler;
}

time_of_day_t time_ofday()
{
    return time_gettimeofday_vec();
}
