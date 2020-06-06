#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

int posix_fadvise(int fd, off_t offset, off_t len, int advice)
{
    long status = syscall4(fd, offset, len,
                           unsigned(advice), SYS_fadvise64);

    if (status == 0)
        return status;

    errno = -status;

    return -1;
}
