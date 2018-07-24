#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int setgid(gid_t gid)
{
    long status = syscall1(gid, SYS_setgid);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
