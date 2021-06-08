#include "time.h"
#include "halt.h"
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

unsigned time_day_of_year(time_of_day_t const& time)
{
    int days[] = {
        31,
        time.year % 400 ? 28 :
        time.year % 100 ? 29 :
        time.year % 4 ? 28 :
        29,
        31,
        30,
        31,
        30,
        31,
        31,
        30,
        31,
        30,
        31
    };

    int yday = 0;
    for (int m = 1; m < time.month; ++m)
        yday += days[m - 1];
    yday += time.day - 1;

    return yday;
}

uint64_t time_unix(time_of_day_t const& time)
{
    uint64_t y = time.fullYear() - 1900;

    return uint64_t(time.second) +
            time.minute * UINT64_C(60) +
            time.hour * UINT64_C(3600) +
            time_day_of_year(time) * UINT64_C(86400) +
            (y - 70) * UINT64_C(365) * 86400 +
            ((y - 69) / 4) * UINT64_C(86400) -
            ((y - 1) / 100) * UINT64_C(86400) +
            ((y + 299) / 400) * UINT64_C(86400);
}

uint64_t time_unix_ms(time_of_day_t const& time)
{
    return time_unix(time) * 1000 + time.centisec * 10;
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

uint64_t time_ns(void)
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

uint64_t nsleep(uint64_t nanosec)
{
    assert(nsleep_vec);
    return nsleep_vec(nanosec);
}

void sleep(int ms)
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
