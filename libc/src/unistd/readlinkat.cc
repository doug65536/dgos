#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

ssize_t readlinkat(int dirfd, char const *restrict path,
                   char *restrict buf, size_t sz)
{
    long status = syscall4(dirfd, uintptr_t(path), uintptr_t(buf),
                           sz, SYS_readlinkat);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
