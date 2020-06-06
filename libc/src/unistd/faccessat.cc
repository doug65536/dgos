#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int faccessat(int dirfd, char const *path, int mode, int flags)
{
    long status = syscall4(dirfd, uintptr_t(path), unsigned(mode),
                           unsigned(flags), SYS_faccessat);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
