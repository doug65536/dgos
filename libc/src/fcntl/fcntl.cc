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

    void *arg = nullptr;

    switch (cmd) {
    case F_DUPFD:
        break;

    case F_DUPFD_CLOEXEC:
        break;

    case F_GETFD:
        break;

    case F_SETFD:
        break;

    case F_GETFL:
        break;

    case F_SETFL:
        break;

    case F_SETLK:
    case F_SETLKW:
    case F_GETLK:
        arg = va_arg(ap, flock *);
        break;

    }

    long status = syscall3(fd, cmd, uintptr_t(arg), SYS_fcntl);
    va_end(ap);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
