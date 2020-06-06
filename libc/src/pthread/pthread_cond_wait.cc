#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
    if (unlikely(c->sig != __PTHREAD_COND_SIG))
        return EINVAL;

    //int status = __futex(&m->owner, FUTEX_OP(__FUTEX_WAKE_OP, -1), -1, )

    return ENOSYS;
}
