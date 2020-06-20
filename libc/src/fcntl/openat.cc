#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>
#include <errno.h>

int openat(int dirfd, char const *path, int flags, ...)
{
    va_list ap;

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int status = syscall4(dirfd, uintptr_t(path), unsigned(flags),
                            mode, SYS_openat);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
