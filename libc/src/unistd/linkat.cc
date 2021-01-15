#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int linkat(int targetdirfd, char const *target,
           int linkdirfd, char const *link, int flags)
{
    long status = syscall5(targetdirfd, uintptr_t(target), linkdirfd,
                           uintptr_t(link), flags, SYS_linkat);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
