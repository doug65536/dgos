#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int dup2(int fd1, int fd2)
{
    return syscall2(fd1, fd2, SYS_dup2);
}
