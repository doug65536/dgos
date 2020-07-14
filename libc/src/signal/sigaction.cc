#include <signal.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>
#include <sys/likely.h>

int sigaction(int signum, struct sigaction const *act, struct sigaction *oldact)
{
    int status = syscall3(unsigned(signum), uintptr_t(act),
                          uintptr_t(oldact), SYS_sigaction);

    if (unlikely(status < 0)) {
        errno = -status;
        return -1;
    }

    return 0;
}
