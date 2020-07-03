#pragma once
#include <sys/types.h>

// 2^31 seconds is 68.05 years
struct timespec {
    // This represents the number of whole seconds of elapsed time.
    int32_t tv_sec;

    // This is the rest of the elapsed time (a fraction of a second),
    // represented as the number of nanoseconds.
    // It is always less than one billion.
    int32_t tv_nsec;
};

#define CLOCK_MONOTONIC 0

int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_getres(clockid_t clk_id, struct timespec *res);
int clock_gettime(clockid_t clk_id, struct timespec *tp);
