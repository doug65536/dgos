#pragma once
#include "types.h"
#include "sys/sys_types.h"

__BEGIN_DECLS

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

int sys_nanosleep(timespec const* req, timespec *rem);

__END_DECLS
