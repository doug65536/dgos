#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

ssize_t pread(int fd, void *buf, size_t sz, off_t ofs)
{
    long status = syscall4(fd, long(buf), sz, ofs, SYS_pread64);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
