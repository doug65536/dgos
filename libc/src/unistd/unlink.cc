#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int unlink(char const *path)
{
    long status = syscall1(uintptr_t(path), SYS_unlink);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
