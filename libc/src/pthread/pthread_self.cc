#include <pthread.h>

__thread int __tid;

pthread_t pthread_self()
{
    pthread_t id = __tid;
    return id;
}

void __pthread_set_tid(pthread_t tid)
{
    __tid = tid;
}
