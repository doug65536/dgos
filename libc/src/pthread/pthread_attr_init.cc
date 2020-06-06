#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_attr_init(pthread_attr_t *a)
{
    if (unlikely(a->sig != __PTHREAD_ATTR_SIG))
        return EINVAL;

    a->detach = false;
    a->guard_sz = 4096;
    a->sched.sched_priority = 0;

    return 0;
}
