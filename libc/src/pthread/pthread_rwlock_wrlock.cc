#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_rwlock_wrlock(pthread_rwlock_t *m)
{
    if (unlikely(m->sig != __PTHREAD_RWLOCK_SIG))
        return EINVAL;

    int c = __atomic_load_n(&m->lock_count, __ATOMIC_ACQUIRE);

    // Spin 1000x first time, 1x after waits
    for (int spins_remain = 1000; ; spins_remain = 1) {
        for ( ; --spins_remain || c == 0; __builtin_ia32_pause()) {
            if (likely(c == 0)) {
                // It is unlocked or there are already readers,
                // attempt to increment read lock and complete
                if (likely(__atomic_compare_exchange_n(
                               &m->lock_count, &c, -1, true,
                               __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)))
                    return 0;
            } else if (c < 0) {
                // There is a writer, poll it again
                c = __atomic_load_n(&m->lock_count, __ATOMIC_ACQUIRE);
            }
        }

        // Spinloop just gave up, wait in the kernel

        int futex_status = __futex(&m->lock_count, __FUTEX_WAIT, c,
                                   nullptr, nullptr, 0);

        // Propagate futex errors to caller
        if (futex_status < 0)
            return -futex_status;
    }
}
