#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int fcntl(int fd, int cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    int result = syscall3(fd, cmd, long(ap), SYS_fcntl);
    va_end(ap);
    return result;
}
