#include <sys/time.h>
#include <sys/syscall_num.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/likely.h>

int clock_gettime(clockid_t clk_id, timespec *tp)
{
    int status = (int)syscall2(clk_id, scp_t(tp), SYS_clock_gettime);

    if (unlikely(status < 0)) {
        errno = -status;
        return -1;
    }

    return 0;
}
