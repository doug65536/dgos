#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_rwlock_unlock(pthread_rwlock_t *m)
{
    if (unlikely(m->sig != __PTHREAD_RWLOCK_SIG))
        return EINVAL;

    int tid = pthread_self();

    if (m->owner != tid) {
        // Releasing read lock
        // Return without waking if there are still readers
        if (likely(__atomic_sub_fetch(
                       &m->lock_count, 1, __ATOMIC_SEQ_CST) != 0))
            return 0;
    } else {
        // Releasing write lock
        m->owner = -1;
        __atomic_store_n(&m->lock_count, 0, __ATOMIC_RELEASE);
    }

    int futex_status = __futex(&m->lock_count, __FUTEX_WAKE, 0,
                               nullptr, nullptr, 0);

    if (unlikely(futex_status < 0))
        return -futex_status;

    return 0;
}
