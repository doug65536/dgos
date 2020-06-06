#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_spin_unlock(pthread_spinlock_t *s)
{
    if (unlikely(s->sig != __PTHREAD_SPINLOCK_SIG))
        return EINVAL;

    int tid = pthread_self();

    int owner = __atomic_load_n(&s->owner, __ATOMIC_ACQUIRE);

    if (unlikely(owner != tid))
        return EPERM;

    __atomic_store_n(&s->owner, -1, __ATOMIC_RELEASE);

    return 0;
}
