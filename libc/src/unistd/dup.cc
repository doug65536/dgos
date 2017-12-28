#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int dup(int fd)
{
    return syscall1(fd, SYS_dup);
}
