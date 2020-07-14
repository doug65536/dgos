#include <signal.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>
#include <sys/likely.h>

int raise(int sig)
{
    int status = syscall2(-1, sig, SYS_kill);

    if (unlikely(status < 0)) {
        errno = -status;
        return -1;
    }

    return 0;
}
