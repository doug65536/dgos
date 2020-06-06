#include <pthread.h>
#include <errno.h>

int pthread_barrier_wait(pthread_barrier_t *)
{
    return ENOSYS;
}
