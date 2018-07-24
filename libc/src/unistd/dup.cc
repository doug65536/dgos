#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int dup(int fd)
{
    long status = syscall1(fd, SYS_dup);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
