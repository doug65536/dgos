#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_mutex_unlock(pthread_mutex_t *m)
{
    if (unlikely(m->sig != __PTHREAD_MUTEX_SIG))
        return EINVAL;

    int tid = pthread_self();

    if (unlikely(m->owner != tid))
        return EPERM;

    // Decrement recursion count if recursions have nested
    if (unlikely(m->recursions > 0)) {
        --m->recursions;
        return 0;
    }

    // Release ownership
    __atomic_store_n(&m->owner, -1, __ATOMIC_RELEASE);

    int status = __futex(&m->owner, __FUTEX_WAKE, 1, nullptr, nullptr, 0);

    if (unlikely(status < 0))
        return -status;

    return 0;
}
