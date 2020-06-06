#include <pthread.h>
#include <errno.h>

int pthread_atfork(void (*)(), void (*)(), void (*)())
{
    errno = ENOSYS;
    return -1;
}
