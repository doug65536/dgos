#include "time.h"
#include "cpu/halt.h"
#include "cpu/control_regs.h"
#include "thread.h"
#include "printk.h"
#include "export.h"

// Monotonically increasing nanosecond clock
static uint64_t time_ns_dummy(void);

// Returns the number of microseconds actually elapsed, if possible
// otherwise returns passed value
static uint64_t nsleep_dummy(uint64_t microsec);

static uint64_t (*time_ns_vec)(void) = time_ns_dummy;
static uint64_t (*nsleep_vec)(uint64_t microsec) = nsleep_dummy;

static time_ofday_handler_t time_gettimeofday_vec;

// Provides a handler for time_ns until the timer is initialized
static uint64_t time_ns_dummy(void)
{
    return 0;
}

// Provides a handler for nsleep until the timer is initialized
static uint64_t nsleep_dummy(uint64_t)
{
    return 0;
}

void time_ns_set_handler(uint64_t (*vec)(void))
{
    time_ns_vec = vec;
}

EXPORT uint64_t time_ns(void)
{
    return time_ns_vec();
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
