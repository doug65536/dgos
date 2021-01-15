#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

pid_t getpgid(pid_t gid)
{
    long status = syscall1(gid, SYS_getpgid);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
