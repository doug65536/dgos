#include <signal.h>
#include <sys/likely.h>
#include <limits.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int sigaddset(sigset_t *set, int sig)
{
    if (unlikely(unsigned(sig) >= (sizeof(uint64_t) * 8))) {
        errno = EINVAL;
        return -1;
    }

    *set |= sigset_t(1) << sig;

    return 0;
}
