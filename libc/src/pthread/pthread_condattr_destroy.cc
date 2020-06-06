#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_condattr_destroy(pthread_condattr_t *)
{
    return ENOSYS;
}
