#pragma once
#include "assert.h"
#include "spinlock.h"
#include "thread.h"
#include "type_traits.h"
#include "cpu/control_regs.h"

#define MUTEX_DEBUG 0
#if MUTEX_DEBUG
#define MUTEX_DTRACE(...) printdbg("mutex: " __VA_ARGS__)
#else
#define MUTEX_DTRACE(...) ((void)0)
#endif

#define DEBUG_CONDVAR 0
#if DEBUG_CONDVAR
#define CONDVAR_DTRACE(...) printdbg("condvar: " __VA_ARGS__)
#else
#define CONDVAR_DTRACE(...) ((void)0)
#endif

__BEGIN_DECLS

struct thread_wait_link_t;
struct condition_var_t;
struct thread_wait_t;
struct mutex_t;

// Link in chain of waiters
struct thread_wait_link_t {
    thread_wait_link_t * next;
    thread_wait_link_t * prev;
};

// An instance of this is maintained by waiting threads
struct thread_wait_t {
    thread_wait_link_t link;

    thread_t thread;
};

struct condition_var_t {
    // Linked list of threads waiting for the condition variable
    thread_wait_link_t link;

    // This lock must be held while adding a
    // thread_wait_t to the wait list
    spinlock_t lock;

    uint32_t reserved;
};

C_ASSERT(sizeof(condition_var_t) == 8 * 3);

struct mutex_t {
    // Linked list of threads waiting for the mutex
    thread_wait_link_t link;

    thread_t owner;
    int spin_count;

    // This lock must be held while updating the list
    spinlock_t lock;
    uint32_t reserved;
};

C_ASSERT(sizeof(mutex_t) == 4 * 8);

struct rwlock_t {
    // Chain of writer waiters
    thread_wait_link_t ex_link;

    // Chain of reader waiters
    thread_wait_link_t sh_link;

    // negative=thread id of exclusive owner, 0=unlocked, positive=reader count
    int reader_count;
    int spin_count;

    spinlock_t lock;
};

KERNEL_API void mutex_init(mutex_t *mutex);
KERNEL_API void mutex_destroy(mutex_t *mutex);
KERNEL_API int mutex_held(mutex_t *mutex);
KERNEL_API bool mutex_try_lock(mutex_t *mutex);
KERNEL_API bool mutex_lock(mutex_t *mutex);
KERNEL_API bool mutex_try_lock_until(mutex_t *mutex,
                                     uint64_t timeout_time);
KERNEL_API void mutex_unlock(mutex_t *mutex);

KERNEL_API void rwlock_init(rwlock_t *rwlock);
KERNEL_API void rwlock_destroy(rwlock_t *rwlock);
KERNEL_API bool rwlock_ex_try_lock(rwlock_t *rwlock);
KERNEL_API bool rwlock_ex_lock(rwlock_t *rwlock,
                               uint64_t timeout_time = UINT64_MAX);
KERNEL_API bool rwlock_upgrade(rwlock_t *rwlock,
                               uint64_t timeout_time = UINT64_MAX);
KERNEL_API bool rwlock_sh_try_lock(rwlock_t *rwlock);
KERNEL_API bool rwlock_sh_lock(rwlock_t *rwlock,
                               uint64_t timeout_time = UINT64_MAX);
KERNEL_API void rwlock_ex_unlock(rwlock_t *rwlock);
KERNEL_API void rwlock_sh_unlock(rwlock_t *rwlock);
KERNEL_API bool rwlock_have_ex(rwlock_t *rwlock);

KERNEL_API void condvar_init(condition_var_t *var);
KERNEL_API void condvar_destroy(condition_var_t *var);
KERNEL_API bool condvar_wait_mutex(condition_var_t *var, mutex_t *mutex,
                                   uint64_t timeout_time = UINT64_MAX);
KERNEL_API void condvar_wake_one(condition_var_t *var);
KERNEL_API void condvar_wake_all(condition_var_t *var);
KERNEL_API void condvar_wake_n(condition_var_t *var, size_t n);

//bool condvar_wait_spinlock(condition_var_t *var, spinlock_t *spinlock,
//                           uint64_t timeout_time = UINT64_MAX);
//bool condvar_wait_ticketlock(condition_var_t *var, ticketlock_t *spinlock,
//                             uint64_t timeout_time = UINT64_MAX);

//bool condvar_wait_mcslock(condition_var_t *var,
//                          mcs_queue_ent_t * volatile *root,
//                          mcs_queue_ent_t * node,
//                          uint64_t timeout_time = UINT64_MAX);


KERNEL_API void thread_wait_add(thread_wait_link_t *root,
                                thread_wait_link_t *node);
KERNEL_API thread_wait_link_t *thread_wait_del(thread_wait_link_t *node);

__END_DECLS

template<typename T>
bool condvar_wait_ex(condition_var_t *var, T& lock_upd,
                            uint64_t timeout_time)
{
    uintptr_t result;

    // Lock the condition variable
    spinlock_lock(&var->lock);

    // Uninterruptible code ahead
    cpu_scoped_irq_disable irq_dis;

    thread_wait_t wait;
    thread_wait_add(&var->link, &wait.link);

    // Unlock whatever lock is protecting the condition
    lock_upd.unlock_noirq();

    // Atomically unlock the condition variable and suspend
    // note: returns with var->lock unlocked!
    CONDVAR_DTRACE("%p: Suspending\n", (void*)&wait);
    result = thread_sleep_release(&var->lock, &wait.thread, timeout_time);
    CONDVAR_DTRACE("%p: Awoke\n", (void*)&wait);

    // (do not) Release (not) reacquired condition variable lock
    //spinlock_unlock(&var->lock);

    // Sanely disconnected nodes will have nulled links
    if (result) {
        assert(wait.link.next == nullptr);
        assert(wait.link.prev == nullptr);
    } else {
        // Reacquire condition variable lock to remove this thread from the
        // notification list
        spinlock_lock(&var->lock);

        // Possible that condition was notified after timer expiry
        if (likely(wait.link.next))
            thread_wait_del(&wait.link);
        else
            result = true;

        spinlock_unlock(&var->lock);
    }

    // Reacquire lock protecting condition before returning
    lock_upd.lock_noirq();

    return result;
}
