#pragma once

struct timespec {
    // This represents the number of whole seconds of elapsed time.
    long tv_sec;

    // This is the rest of the elapsed time (a fraction of a second),
    // represented as the number of nanoseconds.
    // It is always less than one billion.
    long tv_nsec;
};