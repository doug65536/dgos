#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int chown(char const *path, uid_t uid, gid_t gid)
{
    long status = syscall3(long(path), long(uid), long(gid), SYS_chown);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
