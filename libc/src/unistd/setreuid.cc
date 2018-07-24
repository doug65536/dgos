#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int setreuid(uid_t ruid, uid_t euid)
{
    long status = syscall2(ruid, euid, SYS_setresuid);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
