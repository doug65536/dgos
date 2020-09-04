#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/likely.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int pthread_detach(pthread_t tid)
{
    scp_t status = syscall1(tid, SYS_detach);

    if (unlikely(status < 0))
        return -(int)status;

    return 0;
}
