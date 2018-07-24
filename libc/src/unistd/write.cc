#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

ssize_t write(int fd, void const *buf, size_t n)
{
    long status = syscall3(long(fd), long(buf), long(n), SYS_write);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
