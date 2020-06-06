#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_equal(pthread_t, pthread_t)
{
    return ENOSYS;
}
