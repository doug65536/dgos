#include <pthread.h>
#include <errno.h>

int pthread_barrier_destroy(pthread_barrier_t *)
{
    return ENOSYS;
}
