#include <pthread.h>
#include <errno.h>

int pthread_barrierattr_destroy(pthread_barrierattr_t *)
{
    return ENOSYS;
}
