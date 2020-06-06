#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_attr_setguardsize(pthread_attr_t *a, size_t size)
{
    if (unlikely(a->sig != __PTHREAD_ATTR_SIG))
        return EINVAL;

    a->guard_sz = size;

    return 0;
}
