#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int fchown(int fd, uid_t uid, gid_t gid)
{
    long status = syscall3(fd, uid, gid, SYS_fchown);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
