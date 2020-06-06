#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_mutex_init(pthread_mutex_t *m, pthread_mutexattr_t const *a)
{
    if (unlikely(m->sig == __PTHREAD_MUTEX_SIG))
        return EINVAL;

    *m = PTHREAD_MUTEX_INITIALIZER;

    return 0;
}
