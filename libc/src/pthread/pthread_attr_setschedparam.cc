#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_attr_setschedparam(pthread_attr_t *a, const sched_param *p)
{
    if (unlikely(a->sig != __PTHREAD_ATTR_SIG))
        return EINVAL;

    a->sched.sched_priority = p->sched_priority;

    return 0;
}
