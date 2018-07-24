#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int truncate(char const *path, off_t sz)
{
    long status = syscall2(long(path), sz, SYS_truncate);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
