#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_condattr_setclock(pthread_condattr_t *, clockid_t)
{
    return ENOSYS;
}
