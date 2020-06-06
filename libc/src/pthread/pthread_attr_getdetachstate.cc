#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_attr_getdetachstate(pthread_attr_t const *a, int *ret)
{
    if (unlikely(a->sig != __PTHREAD_ATTR_SIG))
        return EINVAL;

    *ret = a->detach;

    return 0;
}
