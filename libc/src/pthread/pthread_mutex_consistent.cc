#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>

#define MAX_THREADS 512

int pthread_mutex_consistent(pthread_mutex_t *m)
{
    return m->sig == __PTHREAD_MUTEX_SIG &&
            m->owner >= -1 && m->owner < MAX_THREADS &&
            m->recursions >= -1;
}
