#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/syscall_num.h>
#include <sys/syscall.h>
#include <errno.h>

int ioctl(int __fd, unsigned long int __request, ...)
{
    va_list ap;
    va_start(ap, __request);
    char * restrict argp = va_arg(ap, char *);
    va_end(ap);

    long result = syscall3(__fd, __request, long(argp), SYS_sys_ioctl);

    if (result >= 0)
        return int(result);

    errno = errno_t(-result);

    return -1;
}
