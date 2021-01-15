#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int unlinkat(int dirfd, char const *path, int flags)
{
    long status = syscall3(dirfd, uintptr_t(path),
                           unsigned(flags), SYS_unlinkat);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
