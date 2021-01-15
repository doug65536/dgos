#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int ftruncate(int fd, off_t sz)
{
    long status = syscall2(fd, sz, SYS_ftruncate);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
