#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

ssize_t readlink(char const *restrict path, char *restrict buf, size_t sz)
{
    long status = syscall3(uintptr_t(path), uintptr_t(buf), sz, SYS_readlink);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
