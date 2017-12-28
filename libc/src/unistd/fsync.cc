#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int fsync(int fd)
{
    return syscall1(fd, SYS_fsync);
}
