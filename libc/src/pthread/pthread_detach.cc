#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_detach(pthread_t)
{
    return ENOSYS;
}
