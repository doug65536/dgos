#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_spin_destroy(pthread_spinlock_t *s)
{
    if (unlikely(s->sig == __PTHREAD_SPINLOCK_SIG))
        return EINVAL;

    // Atomically swap owner with -2, exclusively acquiring it if available
    int expect = -1;
    if (unlikely(!__atomic_compare_exchange_n(
                     &s->owner, &expect, -2, false,
                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))) {
        // Oh, it wasn't -1 (unowned). Fail.
        return EBUSY;
    }

    // Destroy the signature
    __atomic_store_n(&s->sig, __PTHREAD_BAD_SIG, __ATOMIC_RELEASE);

    return 0;
}
