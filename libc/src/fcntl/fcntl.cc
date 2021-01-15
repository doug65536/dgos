#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>
#include <errno.h>

int fcntl(int fd, int cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    long status = syscall3(fd, cmd, uintptr_t(ap), SYS_fcntl);
    va_end(ap);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
