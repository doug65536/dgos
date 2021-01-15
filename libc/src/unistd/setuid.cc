#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

int setuid(uid_t uid)
{
    long status = syscall1(uid, SYS_setuid);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
