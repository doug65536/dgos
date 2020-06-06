#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_rwlock_trywrlock(pthread_rwlock_t *m)
{
    if (unlikely(m->sig != __PTHREAD_RWLOCK_SIG))
        return EINVAL;

    int c = __atomic_load_n(&m->lock_count, __ATOMIC_ACQUIRE);

    if (likely(c == 0)) {
        // It is unlocked or there are already readers,
        // attempt to increment read lock and complete
        if (likely(__atomic_compare_exchange_n(
                       &m->lock_count, &c, -1, true,
                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)))
            return 0;
    }

    return EBUSY;
}
