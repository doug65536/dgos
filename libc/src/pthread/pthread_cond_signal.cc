#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_cond_signal(pthread_cond_t *c)
{
    if (unlikely(c->sig != __PTHREAD_COND_SIG))
        return EINVAL;

    int status = __futex((int*)&c->sig, __FUTEX_WAKE, 1, nullptr, nullptr, 0);

    if (unlikely(status < 0))
        return -status;

    return 0;
}
