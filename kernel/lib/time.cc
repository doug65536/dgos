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
static uint64_t nsleep_dummy(uint64_t microsec);

static uint64_t (*time_ms_vec)(void) = time_ms_dummy;
static uint64_t (*nsleep_vec)(uint64_t microsec) = nsleep_dummy;

static time_ofday_handler_t time_gettimeofday_vec;

// Provides a handler for time_ms until the timer is initialized
static uint64_t time_ms_dummy(void)
{
    return 0;
}

// Provides a handler for nsleep until the timer is initialized
static uint64_t nsleep_dummy(uint64_t)
{
    return 0;
}

void time_ms_set_handler(uint64_t (*vec)(void))
{
    time_ms_vec = vec;
}

EXPORT uint64_t time_ms(void)
{
    return time_ms_vec();
}

void nsleep_set_handler(uint64_t (*vec)(uint64_t nanosec))
{
    nsleep_vec = vec;
}

EXPORT uint64_t nsleep(uint64_t nanosec)
{
    return nsleep_vec(nanosec);
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
