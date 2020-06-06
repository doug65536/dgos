#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_mutexattr_destroy(pthread_mutexattr_t *)
{
    return 0;
}
