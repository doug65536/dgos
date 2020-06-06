#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_condattr_init(pthread_condattr_t *)
{
    return ENOSYS;
}
