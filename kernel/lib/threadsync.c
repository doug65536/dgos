#include "threadsync.h"
#include "thread.h"
#include "assert.h"
#include "cpu/atomic.h"
#include "printk.h"
#include "time.h"

#define DEBUG_MUTEX 0
#if DEBUG_MUTEX
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

#define SPINCOUNT_MAX   512
#define SPINCOUNT_MIN   16

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

void mutex_init(mutex_t *mutex)
{
    mutex->owner = -1;
    mutex->lock = 0;
    mutex->spin_count = 64;
    mutex->link.next = &mutex->link;
    mutex->link.prev = &mutex->link;
}

void mutex_lock(mutex_t *mutex)
{
    for (int spin = 0; spin < mutex->spin_count &&
         mutex->owner >= 0; ++spin)
        pause();

    // Lock the mutex to acquire it or manipulate wait chain
    spinlock_hold_t hold;
    hold = spinlock_lock_noirq(&mutex->lock);

    if (mutex->owner < 0) {
        // Take ownership
        mutex->owner = thread_get_id();

        MUTEX_DTRACE("Took ownership of %p\n", (void*)mutex);

        // Increase spin count
        if (mutex->spin_count < SPINCOUNT_MAX)
            ++mutex->spin_count;

        atomic_barrier();
    } else {
        // Mutex is owned
        thread_wait_t wait;

        // Decrease spin count
        if (mutex->spin_count > SPINCOUNT_MIN)
            --mutex->spin_count;

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
    }

    // Release lock
    spinlock_unlock_noirq(&mutex->lock, &hold);
}

void mutex_unlock(mutex_t *mutex)
{
    spinlock_hold_t hold;
    hold = spinlock_lock_noirq(&mutex->lock);

    atomic_barrier();
    assert(mutex->owner == thread_get_id());

    // See if any threads are waiting
    if (mutex->link.next != &mutex->link) {
        // Wake up the first waiter
        MUTEX_DTRACE("Mutex unlock waking waiter\n");
        thread_wait_t *waiter = (void*)mutex->link.next;
        thread_wait_del(&waiter->link);
        mutex->owner = waiter->thread;
        spinlock_unlock_noirq(&mutex->lock, &hold);
        thread_resume(waiter->thread);
    } else {
        // No waiters
        mutex->owner = -1;
        atomic_barrier();
        spinlock_unlock_noirq(&mutex->lock, &hold);
    }
}

void mutex_lock_noyield(mutex_t *mutex)
{
    spinlock_hold_t hold;
    for (int done = 0; ; pause()) {
        if (mutex->owner >= 0)
            continue;

        hold = spinlock_lock_noirq(&mutex->lock);

        if (mutex->owner < 0) {
            mutex->owner = thread_get_id();
            done = 1;
        }

        spinlock_unlock_noirq(&mutex->lock, &hold);

        if (done)
            break;
    }
}

void mutex_destroy(mutex_t *mutex)
{
    assert(mutex->link.next == mutex->link.prev);
}

void condvar_init(condition_var_t *var)
{
    var->lock = 0;
    var->link.next = &var->link;
    var->link.prev = &var->link;
    atomic_barrier();
}

void condvar_destroy(condition_var_t *var)
{
    if (var->link.prev != &var->link) {
        spinlock_hold_t hold = spinlock_lock_noirq(&var->lock);
        for (thread_wait_link_t volatile *node = var->link.next;
             node != &var->link; node = thread_wait_del(node));
        spinlock_unlock_noirq(&var->lock, &hold);
    }
    assert(var->link.next == &var->link);
    assert(var->link.prev == &var->link);
}

void condvar_wait(condition_var_t *var, mutex_t *mutex)
{
    assert(mutex->owner >= 0);

    spinlock_hold_t hold;
    hold = spinlock_lock_noirq(&var->lock);

    thread_wait_t wait;
    thread_wait_add(&var->link, &wait.link);

    mutex_unlock(mutex);
    CONDVAR_DTRACE("%p: Suspending\n", (void*)&wait);
    thread_suspend_release(&var->lock, &wait.thread);
    CONDVAR_DTRACE("%p: Awoke\n", (void*)&wait);
    mutex_lock(mutex);

    assert(wait.link.next == 0);
    assert(wait.link.prev == 0);

    spinlock_unlock_noirq(&var->lock, &hold);
}

void condvar_wake_one(condition_var_t *var)
{
    spinlock_hold_t hold;
    hold = spinlock_lock_noirq(&var->lock);
    atomic_barrier();

    thread_wait_t volatile *wait = (void*)var->link.next;
    if ((void*)wait != (void*)&var->link) {
        CONDVAR_DTRACE("%p: Removing wait\n", (void*)wait);
        thread_wait_del(&wait->link);
        CONDVAR_DTRACE("%p: Waking id %d\n", (void*)wait, wait->thread);
        thread_resume(wait->thread);
    } else {
        CONDVAR_DTRACE("No waiters when waking\n");
    }
    spinlock_unlock_noirq(&var->lock, &hold);
}

void condvar_wake_all(condition_var_t *var)
{
    spinlock_hold_t hold;
    hold = spinlock_lock_noirq(&var->lock);

    for (thread_wait_t *wait = (void*)var->link.next;
         wait != (void*)&var->link;
         wait = (void*)thread_wait_del(&wait->link)) {
        // FIXME: unlink thread_wait_t
        thread_resume(wait->thread);
    }

    spinlock_unlock_noirq(&var->lock, &hold);
}

