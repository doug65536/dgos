#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_key_create(pthread_key_t *, void (*)(void *))
{
    return ENOSYS;
}
