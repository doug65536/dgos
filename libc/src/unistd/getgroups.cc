#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int getgroups(int gidsetsize, gid_t *grouplist)
{
    long status = syscall2(gidsetsize, long(grouplist), SYS_getgroups);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
