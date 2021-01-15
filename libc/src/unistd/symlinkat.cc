#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int symlinkat(char const *target, int dirfd, char const *link)
{
    long status = syscall3(uintptr_t(target), dirfd,
                           uintptr_t(link), SYS_symlinkat);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
