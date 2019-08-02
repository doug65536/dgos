#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

int open(char const *path, int flags, ...)
{
    va_list ap;
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    long status = syscall3(long(path), long(flags), long(mode), SYS_open);

    if (status >= 0)
        return int(status);

    errno = int(-status);

    return -1;
}
