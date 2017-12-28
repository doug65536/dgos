#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

ssize_t write(int fd, void const *buf, size_t n)
{
    return syscall3(long(fd), long(buf), long(n), SYS_write);
}
