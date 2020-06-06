#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_join(pthread_t, void **)
{
    return ENOSYS;
}
