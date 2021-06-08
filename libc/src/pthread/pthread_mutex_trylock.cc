#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>
#include <limits.h>

int pthread_mutex_trylock(pthread_mutex_t *m)
{
    int tid = pthread_self();

    // Check for recursion
    if (unlikely(__atomic_load_n(&m->owner, __ATOMIC_ACQUIRE) == tid)) {
        // Recursion, or invalid

        // Fail if it is not a recursive mutex
        if (likely(m->recursions < 0))
            return -EDEADLK;

        if (m->recursions < INT_MAX)
        ++m->recursions;
        return 0;
    }

    int value;

    for ( ; ; __pause()) {
        value = __atomic_load_n(&m->owner, __ATOMIC_ACQUIRE);

        if (likely(value == -1)) {
            if (likely(__atomic_compare_exchange_n(
                        &m->owner, &value, tid, false,
                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)))
                return 0;
        }

        return EBUSY;
    }
}
