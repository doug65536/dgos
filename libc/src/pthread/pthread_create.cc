#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/likely.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

int pthread_create(pthread_t *result, pthread_attr_t const *attr,
                   void *(*fn)(void *arg), void *arg)
{
    size_t guard_sz = attr ? attr->guard_sz : 0;

    void *stack_st = mmap(nullptr, (8 << 20) + (guard_sz * 2),
                          PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_STACK, -1, 0);
    void *stack_en = (char*)stack_st + (8 << 20);

    void *stack_keep_st = (char*)stack_st + guard_sz;
    void *stack_keep_en = (char*)stack_en - guard_sz;

    if (unlikely(stack_keep_st > stack_st)) {
        madvise(stack_st, guard_sz, MADV_DONTNEED);
        mprotect(stack_st, guard_sz, PROT_NONE);

        madvise(stack_keep_en, guard_sz, MADV_DONTNEED);
        mprotect(stack_keep_en, guard_sz, PROT_NONE);
    }

    __clone(fn, stack_keep_en, 0, arg);

    return ENOSYS;
}
