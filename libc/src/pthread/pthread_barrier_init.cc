#include <pthread.h>
#include <errno.h>

int pthread_barrier_init(pthread_barrier_t *,
                     pthread_barrierattr_t const *, unsigned)
{
    return ENOSYS;
}
