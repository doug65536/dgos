#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_mutex_destroy(pthread_mutex_t *m)
{
    if (unlikely(m->sig != __PTHREAD_MUTEX_SIG))
        return EINVAL;

    // Atomically swap owner with -2, exclusively acquiring it if available
    int expect = -1;
    if (unlikely(!__atomic_compare_exchange_n(
                     &m->owner, &expect, -2, false,
                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))) {
        // Oh, it wasn't -1 (unowned). Fail.
        return EBUSY;
    }

    // Destroy the signature
    __atomic_store_n(&m->sig, __PTHREAD_BAD_SIG, __ATOMIC_RELEASE);

    return 0;
}
