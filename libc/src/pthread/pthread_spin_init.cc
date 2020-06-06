#include <pthread.h>

int pthread_spin_init(pthread_spinlock_t *s, int)
{
    s->sig = __PTHREAD_SPINLOCK_SIG;

    __atomic_store_n(&s->owner, -1, __ATOMIC_RELEASE);

    return 0;
}
