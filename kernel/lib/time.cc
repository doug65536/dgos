#include "time.h"
#include "cpu/halt.h"
#include "cpu/control_regs.h"
#include "thread.h"
#include "printk.h"
#include "export.h"

// Monotonically increasing nanosecond clock
static uint64_t time_ns_dummy();

// Returns the number of microseconds actually elapsed, if possible
// otherwise returns passed value
static uint64_t nsleep_dummy(uint64_t microsec);

static uint64_t (*time_ns_vec)(void) = time_ns_dummy;
static uint64_t (*nsleep_vec)(uint64_t microsec) = nsleep_dummy;

static void (*time_ns_stop_vec)(void) = nullptr;
static void (*nsleep_stop_vec)(void) = nullptr;

static time_ofday_handler_t time_gettimeofday_vec;

// Provides a handler for time_ns until the timer is initialized
static uint64_t time_ns_dummy()
{
    return 0;
}

// Provides a handler for nsleep until the timer is initialized
static uint64_t nsleep_dummy(uint64_t)
{
    return 0;
}

bool time_ns_set_handler(uint64_t (*vec)(), void (*stop)(), bool override)
{
    if (time_ns_vec != time_ns_dummy && !override)
        return false;

    if (time_ns_stop_vec)
        time_ns_stop_vec();

    time_ns_vec = vec;
    time_ns_stop_vec = stop;

    return true;
}

EXPORT uint64_t time_ns(void)
{
    return time_ns_vec();
}

bool nsleep_set_handler(uint64_t (*vec)(uint64_t nanosec), void (*stop)(),
                        bool override)
{
    if (nsleep_vec != nsleep_dummy && !override)
        return false;

    if (nsleep_stop_vec)
        nsleep_stop_vec();

    nsleep_stop_vec = stop;
    nsleep_vec = vec;

    return true;
}

EXPORT uint64_t nsleep(uint64_t nanosec)
{
    assert(nsleep_vec);
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
