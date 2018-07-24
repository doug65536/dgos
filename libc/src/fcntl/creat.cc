#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int creat(char const *path, mode_t mode)
{
    long status = syscall2(long(path), mode, SYS_creat);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
