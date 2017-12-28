#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

ssize_t pread(int fd, void *buf, size_t sz, off_t ofs)
{
    return syscall4(fd, long(buf), sz, ofs, SYS_pread64);
}
