#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int setpgid(pid_t pid, pid_t pgid)
{
    long status = syscall2(pid, pgid, SYS_setpgid);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
