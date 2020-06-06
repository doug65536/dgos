#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_attr_destroy(pthread_attr_t *a)
{
    if (unlikely(a->sig != __PTHREAD_ATTR_SIG))
        return EINVAL;

    // Destroy signature
    a->sig = __PTHREAD_BAD_SIG;

    return 0;
}
