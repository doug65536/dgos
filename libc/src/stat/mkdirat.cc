#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

int mkdirat(int dirfd, char const *path, mode_t mode)
{
    int status = syscall3(dirfd, scp_t(path), unsigned(mode), SYS_mkdirat);

    if (status < 0) {
        errno = -status;
        return -1;
    }

    return status;
}
