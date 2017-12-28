#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

ssize_t pwrite(int fd, const void *buf, size_t sz, off_t ofs)
{
    return syscall4(fd, long(buf), sz, ofs, SYS_pwrite64);
}
