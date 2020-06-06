#include <pthread.h>
#include <errno.h>

int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *, const timespec *)
{
    return ENOSYS;
}
