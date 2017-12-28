#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int close(int fd)
{
    return syscall1(long(fd), SYS_close);
}
