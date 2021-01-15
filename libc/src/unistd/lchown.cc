#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int lchown(char const *path, uid_t uid, gid_t gid)
{
    long status = syscall3(uintptr_t(path), uid, gid, SYS_lchown);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
