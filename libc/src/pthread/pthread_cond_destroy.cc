#include <pthread.h>
#include <errno.h>

int pthread_cond_destroy(pthread_cond_t *)
{
    return ENOSYS;
}
