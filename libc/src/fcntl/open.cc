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

        return openat(AT_FDCWD, path, flags, mode);
    }

    return openat(AT_FDCWD, path, flags);
}
