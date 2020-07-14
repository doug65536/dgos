#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>
#include <errno.h>

int open(char const *path, int flags, ...)
{
    va_list ap;
    va_start(ap, flags);
    mode_t mode = va_arg(ap, mode_t);
    va_end(ap);

    return openat(AT_FDCWD, path, flags, mode);
}
