#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/syscall_num.h>
#include <sys/syscall.h>
#include <sys/likely.h>

int chmod(char const *path, mode_t mode)
{
    int status = syscall2(scp_t(path), unsigned(mode), SYS_chmod);

    if (unlikely(status < 0)) {
        errno = -status;
        return -1;
    }

    return status;
}
