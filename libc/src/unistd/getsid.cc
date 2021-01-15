#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

pid_t getsid(pid_t pid)
{
    long status = syscall1(pid, SYS_getsid);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
