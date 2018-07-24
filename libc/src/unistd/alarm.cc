#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

unsigned alarm(unsigned seconds)
{
    long status = syscall1(long(seconds), SYS_alarm);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
