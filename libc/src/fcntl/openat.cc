#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int openat(int dirfd, char const *path, int oflag, ...)
{
    va_list ap;
    va_start(ap, oflag);
    mode_t mode = va_arg(ap, mode_t);
    va_end(ap);
    return syscall4(long(dirfd), long(path), long(oflag),
                    long(mode), SYS_open);
}
