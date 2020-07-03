#include <sys/time.h>
#include <sys/syscall_num.h>
#include <sys/syscall.h>
#include <errno.h>

int clock_getres(clockid_t clk_id, timespec *res)
{
    res->tv_sec = 0;
    res->tv_nsec = 1;
    return 0;
}
