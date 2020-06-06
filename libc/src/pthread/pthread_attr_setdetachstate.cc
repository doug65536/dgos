#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_attr_setdetachstate(pthread_attr_t *a, int detach)
{
    if (unlikely(a->sig != __PTHREAD_ATTR_SIG))
        return EINVAL;

    a->detach = detach;

    return 0;
}
