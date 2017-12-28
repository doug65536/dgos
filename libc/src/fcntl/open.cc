#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int open(const char *path, int oflag, ...)
{
    va_list ap;
    va_start(ap, oflag);
    mode_t mode = va_arg(ap, mode_t);
    va_end(ap);
    return syscall3(long(path), long(oflag), long(mode), SYS_open);
}
