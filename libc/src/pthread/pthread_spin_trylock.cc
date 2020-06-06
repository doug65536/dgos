#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_spin_trylock(pthread_spinlock_t *s)
{
    if (unlikely(s->sig != __PTHREAD_SPINLOCK_SIG))
        return EINVAL;

    int tid = pthread_self();

    int expect = s->owner;
    if (unlikely(expect != -1 || !__atomic_compare_exchange_n(
                     &s->owner, &expect, tid, false,
                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)))
        return EBUSY;

    return 0;
}
