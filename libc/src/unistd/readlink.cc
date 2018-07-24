#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

ssize_t readlink(char const *restrict path, char *restrict buf, size_t sz)
{
    long status = syscall3(long(path), long(buf), sz, SYS_readlink);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
