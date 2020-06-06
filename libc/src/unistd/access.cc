#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int access(char const *path, int mode)
{
    long status = syscall2(uintptr_t(path), unsigned(mode), SYS_access);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
