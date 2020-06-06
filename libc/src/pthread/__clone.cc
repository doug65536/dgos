#include <pthread.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int __clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg)
{
    long new_tid = syscall4(uintptr_t(fn), uintptr_t(child_stack),
             uint32_t(flags), uintptr_t(arg), SYS_clone);

    if (new_tid > 0)
        return int32_t(uint32_t(new_tid));

    errno = -int(new_tid);

    return -1;
}
