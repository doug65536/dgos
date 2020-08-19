#include <signal.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>
#include <sys/likely.h>

int kill(pid_t pid, int sig)
{
    int status = (int)syscall2(pid, sig, SYS_kill);

    if (unlikely(status < 0)) {
        errno = -status;
        return -1;
    }

    return (int)status;
}
