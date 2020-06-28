#include "time.h"
#include "sys_time.h"
#include "time.h"
#include "thread.h"
#include "user_mem.h"

int sys_nanosleep(timespec const *req, timespec *rem)
{
    int64_t ns = req->tv_sec * 1000000000 + req->tv_nsec;

    int64_t st = time_ns();
    thread_sleep_for(ns / 1000000);
    int64_t en = time_ns();

    en -= st;
    ns -= en;

    if (rem && ns > 0) {
        rem->tv_sec = ns / 1000000000;
        rem->tv_nsec = ns % 1000000000;
    } else if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return ns < 0 ? 0 : -int(errno_t::EINTR);
}

static int clock_copy_timespec_to_user(timespec *user, timespec value)
{
    if (unlikely(!mm_is_user_range(user, sizeof(*user))))
        return -int(errno_t::EFAULT);

    if (unlikely(!mm_copy_user(user, &value, sizeof(*user))))
        return -int(errno_t::EFAULT);

    return 0;
}

int sys_clock_gettime(clockid_t id, timespec *tm)
{
    uint64_t t = time_ns();

    timespec result{};
    result.tv_sec = t / 1000000000;
    result.tv_nsec = t % 1000000000;

    return clock_copy_timespec_to_user(tm, result);
}

int sys_clock_getres(clockid_t id, timespec *tm)
{
    timespec result{};
    result.tv_sec = 0;
    result.tv_nsec = 1;

    return clock_copy_timespec_to_user(tm, result);
}
