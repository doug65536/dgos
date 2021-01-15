#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int fdatasync(int fd)
{
    long status = syscall1(fd, SYS_fdatasync);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
