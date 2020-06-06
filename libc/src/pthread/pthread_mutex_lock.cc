#include <pthread.h>

int pthread_mutex_lock(pthread_mutex_t *m)
{
    return pthread_mutex_timedlock(m, nullptr);
}
