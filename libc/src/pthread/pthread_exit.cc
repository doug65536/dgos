#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>
#include <sys/syscall_num.h>
#include <sys/syscall.h>
#include <stdlib.h>

void pthread_exit(void *exitcode)
{
    syscall2(pthread_self(), scp_t(exitcode), SYS_thread_exit);
    abort();
}
