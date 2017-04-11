#include "threadsync.h"
#include "thread.h"
#include "assert.h"
#include "cpu/atomic.h"
#include "printk.h"
#include "time.h"
#include "export.h"

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

#define SPINCOUNT_MAX   4096
#define SPINCOUNT_MIN   4

static void thread_wait_add(thread_wait_link_t volatile *root,
                            thread_wait_link_t volatile *node)
{
    atomic_barrier();
    thread_wait_link_t volatile *insafter = root->prev;
    node->next = root;
    node->prev = insafter;
    insafter->next = node;
    root->prev = node;
    atomic_barrier();
}

static thread_wait_link_t volatile *thread_wait_del(
        thread_wait_link_t volatile *node)
{
    atomic_barrier();
    thread_wait_link_t volatile *next = node->next;
    thread_wait_link_t volatile *prev = node->prev;
    prev->next = next;
    next->prev = prev;
    node->next = 0;
    node->prev = 0;
    atomic_barrier();
    return next;
}

EXPORT void mutex_init(mutex_t *mutex)
{
    mutex->owner = -1;
    mutex->lock = 0;
    mutex->noyield_waiting = 0;
    mutex->spin_count = spincount_mask &
            (SPINCOUNT_MIN + ((SPINCOUNT_MAX-SPINCOUNT_MIN)>>1));
    mutex->link.next = &mutex->link;
    mutex->link.prev = &mutex->link;
}

EXPORT void mutex_lock(mutex_t *mutex)
{
    assert(mutex->owner != thread_get_id());

    for (int spin = 0; ; pause(), ++spin) {
        // Spin outside lock until spin limit
        if (spin < mutex->spin_count &&
                (mutex->owner >= 0 || mutex->noyield_waiting))
            continue;

        // Lock the mutex to acquire it or manipulate wait chain
        spinlock_lock_noirq(&mutex->lock);

        // Check again inside lock
        if (spin < mutex->spin_count &&
                (mutex->owner >= 0 || mutex->noyield_waiting)) {
            // Racing thread beat us, go back to spinning outside lock
            spinlock_unlock_noirq(&mutex->lock);
            continue;
        }

        if (mutex->owner < 0 && !mutex->noyield_waiting) {
            // Take ownership
            mutex->owner = thread_get_id();

            MUTEX_DTRACE("Took ownership of %p\n", (void*)mutex);

            // Increase spin count
            if (mutex->spin_count < SPINCOUNT_MAX)
                mutex->spin_count -= spincount_mask;

            atomic_barrier();

            break;
        }

        // Mutex is owned
        thread_wait_t wait;

        // Decrease spin count
        if (mutex->spin_count > SPINCOUNT_MIN)
            mutex->spin_count += spincount_mask;

        MUTEX_DTRACE("Adding to waitchain of %p\n", (void*)mutex);

        // Add state to mutex wait chain
        thread_wait_add(&mutex->link, &wait.link);

        MUTEX_DTRACE("Waitchain for %p\n", (void*)mutex);

        // Wait
        thread_suspend_release(&mutex->lock, &wait.thread);

        assert(mutex->lock != 0);
        assert(wait.link.next == 0);
        assert(wait.link.prev == 0);
        assert(mutex->owner == wait.thread);

        break;
    }

    // Release lock
    spinlock_unlock_noirq(&mutex->lock);
}

EXPORT void mutex_unlock(mutex_t *mutex)
{
    spinlock_lock_noirq(&mutex->lock);

    atomic_barrier();
    assert(mutex->owner == thread_get_id());

    // See if any threads are waiting
    if (mutex->link.next != &mutex->link) {
        // Wake up the first waiter
        MUTEX_DTRACE("Mutex unlock waking waiter\n");
        thread_wait_t *waiter = (thread_wait_t*)mutex->link.next;
        thread_wait_del(&waiter->link);
        mutex->owner = waiter->thread;
        spinlock_unlock_noirq(&mutex->lock);
        thread_resume(waiter->thread);
    } else {
        // No waiters
        mutex->owner = -1;
        atomic_barrier();
        spinlock_unlock_noirq(&mutex->lock);
    }
}

EXPORT void mutex_lock_noyield(mutex_t *mutex)
{
    thread_t tid = thread_get_id();
    int noyield_stored = 0;

    for (bool done = false; ; pause()) {
        if (noyield_stored && mutex->owner >= 0)
            continue;

        spinlock_lock_noirq(&mutex->lock);

        if (mutex->noyield_waiting != tid && mutex->noyield_waiting == 0) {
            mutex->noyield_waiting = tid;
            noyield_stored = 1;
        }

        if (mutex->owner < 0) {
            mutex->owner = thread_get_id();

            if (noyield_stored)
                mutex->noyield_waiting = 0;

            done = true;
        }

        spinlock_unlock_noirq(&mutex->lock);

        if (done)
            break;
    }
}

EXPORT void mutex_destroy(mutex_t *mutex)
{
    (void)mutex;
    assert(mutex->link.next == mutex->link.prev);
}

EXPORT int mutex_held(mutex_t *mutex)
{
    return mutex->owner == thread_get_id();
}

EXPORT void condvar_init(condition_var_t *var)
{
    var->lock = 0;
    var->link.next = &var->link;
    var->link.prev = &var->link;
    atomic_barrier();
}

EXPORT void condvar_destroy(condition_var_t *var)
{
    if (var->link.prev != &var->link) {
        spinlock_lock_noirq(&var->lock);
        for (thread_wait_link_t volatile *node = var->link.next;
             node != &var->link; node = thread_wait_del(node));
        spinlock_unlock_noirq(&var->lock);
    }
    assert(var->link.next == &var->link);
    assert(var->link.prev == &var->link);
}

typedef struct condvar_spinlock_t {
    spinlock_t *lock;
} condvar_spinlock_t;

static void condvar_lock_spinlock(void *mutex)
{
    condvar_spinlock_t *state = (condvar_spinlock_t *)mutex;
    spinlock_lock_noirq(state->lock);
}

static void condvar_unlock_spinlock(void *mutex)
{
    condvar_spinlock_t *state = (condvar_spinlock_t *)mutex;
    spinlock_unlock_noirq(state->lock);
}

static void condvar_lock_mutex_noyield(void *mutex)
{
    mutex_lock_noyield((mutex_t *)mutex);
}

static void condvar_lock_mutex(void *mutex)
{
    mutex_lock((mutex_t *)mutex);
}

static void condvar_unlock_mutex(void *mutex)
{
    mutex_unlock((mutex_t *)mutex);
}

static void condvar_wait_ex(condition_var_t *var,
                            void (*lock)(void*),
                            void (*unlock)(void*),
                            void *mutex)
{
    spinlock_lock_noirq(&var->lock);

    thread_wait_t wait;
    thread_wait_add(&var->link, &wait.link);

    unlock(mutex);
    CONDVAR_DTRACE("%p: Suspending\n", (void*)&wait);
    thread_suspend_release(&var->lock, &wait.thread);
    CONDVAR_DTRACE("%p: Awoke\n", (void*)&wait);

    spinlock_unlock_noirq(&var->lock);
    lock(mutex);

    assert(wait.link.next == 0);
    assert(wait.link.prev == 0);
}

EXPORT void condvar_wait_spinlock(condition_var_t *var,
                                  spinlock_t *spinlock)
{
    condvar_spinlock_t state;
    state.lock = spinlock;
    condvar_wait_ex(var, condvar_lock_spinlock,
                    condvar_unlock_spinlock, &state);
}

EXPORT void condvar_wait(condition_var_t *var, mutex_t *mutex)
{
    assert(mutex->owner == thread_get_id());
    condvar_wait_ex(var, condvar_lock_mutex,
                    condvar_unlock_mutex, mutex);
}

EXPORT void condvar_wait_noyield(condition_var_t *var,
                                 mutex_t *mutex)
{
    assert(mutex->owner >= 0);
    condvar_wait_ex(var, condvar_lock_mutex_noyield,
                    condvar_unlock_mutex, mutex);
}

EXPORT void condvar_wake_one(condition_var_t *var)
{
    spinlock_lock_noirq(&var->lock);
    atomic_barrier();

    thread_wait_t volatile *wait = (thread_wait_t*)var->link.next;
    if ((void*)wait != (void*)&var->link) {
        CONDVAR_DTRACE("%p: Removing wait\n", (void*)wait);
        thread_wait_del(&wait->link);
        CONDVAR_DTRACE("%p: Waking id %d\n", (void*)wait, wait->thread);
        thread_resume(wait->thread);
    } else {
        CONDVAR_DTRACE("No waiters when waking\n");
    }
    spinlock_unlock_noirq(&var->lock);
}

EXPORT void condvar_wake_all(condition_var_t *var)
{
    spinlock_lock_noirq(&var->lock);

    for (thread_wait_t *wait = (thread_wait_t*)var->link.next;
         wait != (void*)&var->link;
         wait = (thread_wait_t*)thread_wait_del(&wait->link)) {
        CONDVAR_DTRACE("%p: Waking id %d\n", (void*)wait, wait->thread);
        thread_resume(wait->thread);
    }

    spinlock_unlock_noirq(&var->lock);
}

