#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_attr_getguardsize(pthread_attr_t const *a, size_t *ret)
{
    if (unlikely(a->sig != __PTHREAD_ATTR_SIG))
        return EINVAL;

    *ret = a->guard_sz;

    return 0;
}
