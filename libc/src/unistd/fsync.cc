#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int fsync(int fd)
{
    long status = syscall1(fd, SYS_fsync);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
