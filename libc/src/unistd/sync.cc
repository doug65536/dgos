#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <sys/likely.h>
#include <errno.h>

void sync(void)
{
    long status = syscall0(SYS_sync);

    if (likely(status >= 0))
        return;

    errno = -status;
}
