#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

uid_t geteuid(void)
{
    long status = syscall0(SYS_geteuid);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
