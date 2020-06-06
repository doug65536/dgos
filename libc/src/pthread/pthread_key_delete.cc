#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_key_delete(pthread_key_t)
{
    return ENOSYS;
}
