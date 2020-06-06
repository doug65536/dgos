#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_spin_lock(pthread_spinlock_t *s)
{
    if (unlikely(s->sig != __PTHREAD_SPINLOCK_SIG))
        return EINVAL;

    int tid = pthread_self();

    int expect = s->owner;

    while (unlikely(__atomic_load_n(&expect, __ATOMIC_ACQUIRE) != -1 ||
                    !__atomic_compare_exchange_n(
                        &s->owner, &expect, tid, false,
                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)))
        __builtin_ia32_pause();

    return 0;
}
