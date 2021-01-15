#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int creat(char const *path, mode_t mode)
{
    long status = syscall2(uintptr_t(path), mode, SYS_creat);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
