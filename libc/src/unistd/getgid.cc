#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

gid_t getgid()
{
    long status = syscall0(SYS_getgid);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
