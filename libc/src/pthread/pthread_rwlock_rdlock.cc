#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_rwlock_rdlock(pthread_rwlock_t *m)
{
    return pthread_rwlock_timedrdlock(m, nullptr);
}
