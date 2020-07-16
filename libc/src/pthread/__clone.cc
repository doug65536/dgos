#include <pthread.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/likely.h>

int __clone(void (*bootstrap)(int tid, void *(*fn)(void *arg), void *arg),
            void *child_stack, int flags, void *(*fn)(void *), void *arg)
{
    long new_tid = syscall5(uintptr_t(bootstrap), uintptr_t(child_stack),
             uint32_t(flags), uintptr_t(fn), uintptr_t(arg), SYS_clone);

    if (likely(new_tid > 0))
        return int32_t(uint32_t(new_tid));

    errno = -int(new_tid);

    return -1;
}
