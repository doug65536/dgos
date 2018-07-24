#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int fchownat(int dirfd, char const *path, uid_t uid, gid_t gid, int flags)
{
    long status = syscall5(dirfd, long(path), uid, gid, flags, SYS_fchownat);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
