#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>
#include <sys/likely.h>

int close(int fd)
{
    long status = syscall1(fd, SYS_close);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
