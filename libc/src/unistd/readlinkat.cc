#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

ssize_t readlinkat(int dirfd, char const *restrict path,
                   char *restrict buf, size_t sz)
{
    long status = syscall4(dirfd, long(path), long(buf), sz, SYS_readlinkat);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
