#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int linkat(int targetdirfd, char const *target,
           int linkdirfd, char const *link, int flags)
{
    long status = syscall5(targetdirfd, long(target), linkdirfd,
                           long(link), flags, SYS_linkat);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
