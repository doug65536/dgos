#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int getgroups(int gidsetsize, gid_t *grouplist)
{
    long status = syscall2(gidsetsize, long(grouplist), SYS_getgroups);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
