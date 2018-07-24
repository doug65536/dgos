#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int unlinkat(int dirfd, char const *path, int flags)
{
    long status = syscall3(dirfd, long(path), flags, SYS_unlinkat);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
