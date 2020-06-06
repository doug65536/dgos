#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

ssize_t read(int fd, void *buf, size_t n)
{
    long status = syscall3(fd, uintptr_t(buf), n, SYS_read);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
