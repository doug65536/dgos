#include <pthread.h>
#include <errno.h>
#include <sys/likely.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int __futex(int *uaddr, int futex_op, int val,
            timespec const *timeout, int *uaddr2, int val3)
{
    long result = syscall6(uintptr_t(uaddr),
                           uint32_t(futex_op),
                           uint32_t(val),
                           uintptr_t(timeout),
                           uintptr_t(uaddr2),
                           uint32_t(val3),
                           SYS_futex);

    if (likely(result >= 0))
        return int(result);

    errno = -int(result);
    return -1;
}
