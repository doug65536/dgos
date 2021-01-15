#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

uid_t getuid(void)
{
    long status = syscall0(SYS_getuid);

    if (likely(status >= 0))
        return status;

    errno = -status;

    return -1;
}
