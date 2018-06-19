#include "time.h"
#include "sys_time.h"
#include "time.h"
#include "thread.h"

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
