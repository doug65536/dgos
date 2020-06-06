#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

void *pthread_getspecific(pthread_key_t)
{
    return nullptr;
}
