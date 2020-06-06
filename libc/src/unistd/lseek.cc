#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

off_t lseek(int fd, off_t off, int whence)
{
    long status = syscall3(fd, off, unsigned(whence), SYS_lseek);

    if (status >= 0)
        return status;

    errno = errno_t(-status);

    return -1;
}
