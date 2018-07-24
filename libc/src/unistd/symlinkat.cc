#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int symlinkat(char const *target, int dirfd, char const *link)
{
    long status = syscall3(long(target), dirfd, long(link), SYS_symlinkat);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
