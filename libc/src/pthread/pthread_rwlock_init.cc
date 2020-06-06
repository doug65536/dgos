#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_rwlock_init(pthread_rwlock_t *m, pthread_rwlockattr_t const *a)
{
    if (unlikely(m->sig == __PTHREAD_MUTEX_SIG))
        return EINVAL;

    *m = PTHREAD_RWLOCK_INITIALIZER;

    return 0;
}
