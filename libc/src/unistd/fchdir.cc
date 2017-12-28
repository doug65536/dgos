#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>

int fchdir(int dirfd)
{
    return syscall1(dirfd, SYS_fchdir);
}
