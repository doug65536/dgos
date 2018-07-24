#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int dup2(int fd1, int fd2)
{
    long status = syscall2(fd1, fd2, SYS_dup2);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
