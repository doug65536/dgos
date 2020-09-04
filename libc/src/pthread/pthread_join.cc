#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/likely.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int pthread_join(pthread_t tid, void **exit_value)
{
    scp_t status = syscall2(tid, scp_t(exit_value), SYS_join);

    if (unlikely(status < 0))
        return -status;

    return 0;
}
