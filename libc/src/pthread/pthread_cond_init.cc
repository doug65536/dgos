#include <pthread.h>
#include <errno.h>

int pthread_cond_init(pthread_cond_t *c, pthread_condattr_t const *a)
{
    *c = PTHREAD_COND_INITIALIZER;

    return 0;
}
