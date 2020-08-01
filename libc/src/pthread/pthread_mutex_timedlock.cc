#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>
#include <limits.h>

int pthread_mutex_timedlock(pthread_mutex_t *m, timespec const *timeout_time)
{
    if (unlikely(m->sig != __PTHREAD_MUTEX_SIG))
        return EINVAL;

    int tid = pthread_self();

    int value = __atomic_load_n(&m->owner, __ATOMIC_ACQUIRE);

    // Check for recursion
    if (unlikely(value == tid)) {
        // Recursion, or invalid

        // Fail if it is not a recursive mutex
        if (likely(m->recursions < 0))
            return EDEADLK;

        if (unlikely(m->recursions == INT_MAX))
            return EAGAIN;

        ++m->recursions;

        return 0;
    }

    // Spin 1000x first time, 1x after waits
    for (int spins_remain = 1000; ; spins_remain = 1) {
        for ( ; value == -1 || --spins_remain; __builtin_ia32_pause()) {
            if (likely(value == -1)) {
                if (likely(__atomic_compare_exchange_n(
                            &m->owner, &value, tid, true,
                            __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)))
                    return 0;
            } else {
                value = __atomic_load_n(&m->owner, __ATOMIC_ACQUIRE);
            }
        }

        // Spinloop just gave up, wait in the kernel

        int futex_status = __futex(&m->owner, __FUTEX_WAIT,
                                   value, timeout_time, nullptr, 0);

        // Propagate futex errors to caller
        if (unlikely(futex_status < 0))
            return -futex_status;
    }
}
