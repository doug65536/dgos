#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_create(pthread_t *, pthread_attr_t const *, void *(*)(void *), void *)
{
    return ENOSYS;
}
