#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int setregid(gid_t rgid, gid_t egid)
{
    long status = syscall2(rgid, egid, SYS_setregid);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
