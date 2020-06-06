#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_attr_getschedparam(pthread_attr_t const *a, sched_param *ret)
{
    if (unlikely(a->sig != __PTHREAD_ATTR_SIG))
        return EINVAL;

    ret->sched_priority = a->sched.sched_priority;

    return 0;
}
