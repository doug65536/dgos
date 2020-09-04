#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/likely.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>

__attribute__((__noreturn__))
static void pthread_bootstrap(int tid, void *(fn)(void*), void *arg)
{
#ifdef __x86_64__
    __asm__ ( ".cfi_undefined rip\n" );
#else
    __asm__ ( ".cfi_undefined eip\n" );
#endif

    __pthread_set_tid(tid);
    void *result = fn(arg);
    pthread_exit(result);
}

int pthread_create(pthread_t *result, pthread_attr_t const *attr,
                   void *(*fn)(void *arg), void *arg)
{
    size_t guard_sz = attr ? attr->guard_sz : 0;

    bool detach = attr ? attr->detach : false;

    // 8MB
    size_t stack_sz = 8 << 20;

    void *stack_st = mmap(nullptr, stack_sz + (guard_sz * 2),
                          PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_STACK, -1, 0);
    void *stack_en = (char*)stack_st + stack_sz;

    void *stack_keep_st = (char*)stack_st + guard_sz;
    void *stack_keep_en = (char*)stack_en - guard_sz;

    if (unlikely(stack_keep_st > stack_st)) {
        madvise(stack_st, guard_sz, MADV_DONTNEED);
        mprotect(stack_st, guard_sz, PROT_NONE);

        madvise(stack_keep_en, guard_sz, MADV_DONTNEED);
        mprotect(stack_keep_en, guard_sz, PROT_NONE);
    }

    int flags = detach ? __CLONE_FLAGS_DETACHED : 0;

    int tid_negerr = __clone(pthread_bootstrap, stack_keep_en, flags, fn, arg);

    if (unlikely(tid_negerr < 0))
        return -tid_negerr;

    *result = tid_negerr;

    if (attr && attr->detach)
        pthread_detach(*result);

    return 0;
}
