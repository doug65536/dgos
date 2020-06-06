#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_condattr_getclock(pthread_condattr_t const *, clockid_t *)
{
    return ENOSYS;
}
