#pragma once
#include "assert.h"
#include "cpu/spinlock.h"
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

void mutex_init(mutex_t *mutex);
void mutex_destroy(mutex_t *mutex);
int mutex_held(mutex_t *mutex);
bool mutex_try_lock(mutex_t *mutex);
bool mutex_lock(mutex_t *mutex);
bool mutex_try_lock_until(mutex_t *mutex, uint64_t timeout_time);
void mutex_unlock(mutex_t *mutex);

void rwlock_init(rwlock_t *rwlock);
void rwlock_destroy(rwlock_t *rwlock);
bool rwlock_ex_try_lock(rwlock_t *rwlock);
bool rwlock_ex_lock(rwlock_t *rwlock, uint64_t timeout_time = UINT64_MAX);
bool rwlock_upgrade(rwlock_t *rwlock, uint64_t timeout_time = UINT64_MAX);
bool rwlock_sh_try_lock(rwlock_t *rwlock);
bool rwlock_sh_lock(rwlock_t *rwlock, uint64_t timeout_time = UINT64_MAX);
void rwlock_ex_unlock(rwlock_t *rwlock);
void rwlock_sh_unlock(rwlock_t *rwlock);
bool rwlock_have_ex(rwlock_t *rwlock);

void condvar_init(condition_var_t *var);
void condvar_destroy(condition_var_t *var);
bool condvar_wait_mutex(condition_var_t *var, mutex_t *mutex,
                        uint64_t timeout_time = UINT64_MAX);
void condvar_wake_one(condition_var_t *var);
void condvar_wake_all(condition_var_t *var);
void condvar_wake_n(condition_var_t *var, size_t n);

//bool condvar_wait_spinlock(condition_var_t *var, spinlock_t *spinlock,
//                           uint64_t timeout_time = UINT64_MAX);
//bool condvar_wait_ticketlock(condition_var_t *var, ticketlock_t *spinlock,
//                             uint64_t timeout_time = UINT64_MAX);

//bool condvar_wait_mcslock(condition_var_t *var,
//                          mcs_queue_ent_t * volatile *root,
//                          mcs_queue_ent_t * node,
//                          uint64_t timeout_time = UINT64_MAX);

__END_DECLS

void thread_wait_add(thread_wait_link_t *root, thread_wait_link_t *node);
thread_wait_link_t *thread_wait_del(thread_wait_link_t *node);

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

class scoped_rwlock_t {
public:
    enum locktype_t {
        exclusive = -1,
        none = 0,
        shared = 1,

        writer = exclusive,
        reader = shared
    };

    scoped_rwlock_t(rwlock_t &init_lock, locktype_t init_type)
        : lock(&init_lock)
        , type(none)
    {
        update(init_type);
    }

    scoped_rwlock_t(scoped_rwlock_t const &) = delete;

    scoped_rwlock_t(scoped_rwlock_t&& rhs)
        : lock(rhs.lock)
        , type(rhs.type)
    {
        rhs.detach();
    }

    ~scoped_rwlock_t()
    {
        update(none);
        detach();
    }

    void update(locktype_t new_type)
    {
        switch (type) {
        case none:
            switch (new_type) {
            case exclusive:
                rwlock_ex_lock(lock);
                type = exclusive;
                break;
            case shared:
                rwlock_sh_lock(lock);
                type = shared;
                break;
            case none:
                break;
            }
            break;

        case exclusive:
            switch (new_type) {
            case exclusive:
                break;
            case shared:
                rwlock_ex_unlock(lock);
                rwlock_sh_lock(lock);
                type = shared;
                break;
            case none:
                rwlock_ex_unlock(lock);
                type = none;
                break;
            }
            break;

        case shared:
            switch (new_type) {
            case exclusive:
                rwlock_upgrade(lock);
                type = exclusive;
                break;
            case shared:
                break;
            case none:
                rwlock_sh_unlock(lock);
                break;
            }
            break;

        default:
            assert(!"Corrupt current type!");
        }
    }

    void detach()
    {
        lock = nullptr;
        type = none;
    }

    void upgrade()
    {
        update(exclusive);
    }

    void lock_exclusive()
    {
        update(exclusive);
    }

    void lock_shared()
    {
        update(shared);
    }

    void lock_write()
    {
        update(exclusive);
    }

    void lock_read()
    {
        update(shared);
    }

    void unlock()
    {
        update(none);
    }

    bool is_exclusive() const
    {
        return type == exclusive;
    }

    bool is_shared() const
    {
        return type == shared;
    }

    bool is_locked() const
    {
        return type != none;
    }

    bool is_attached() const
    {
        return lock != nullptr;
    }

private:
    rwlock_t *lock;
    locktype_t type;
};
