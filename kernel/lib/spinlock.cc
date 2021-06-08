#include "spinlock.h"
#include "atomic.h"
#include "assert.h"
#include "cpu/control_regs.h"
#include "printk.h"
#include "export.h"
#include "thread.h"
#include "time.h"

#define DEBUG_MCSLOCK 0
#if DEBUG_MCSLOCK
#define MCSLOCK_TRACE(...) printdbg("mcslock: " __VA_ARGS__)
#else
#define MCSLOCK_TRACE(...) ((void)0)
#endif

//
// Exclusive lock. 0 is unlocked, 1 is locked

// Spin to acquire lock, return having entered critical section
_hot
void spinlock_lock(spinlock_t *lock)
{
    cs_enter();

    // Try to immediately acquire lock, otherwise go into polling
    while (unlikely(atomic_cmpxchg(lock, 0, 1) != 0))
        cpu_wait_value(lock, 0);
}

// Returns 1 with interrupts disabled if lock was acquired
// Returns 0 with interrupts preserved if lock was not acquired
bool spinlock_try_lock(spinlock_t *lock)
{
    cs_enter();

    // Try to immediately acquire the lock, otherwise fail
    if (atomic_cmpxchg(lock, 0, 1) != 0) {
        cs_leave();

        return false;
    }

    return true;
}

// Returns 1 with interrupts disabled if lock was acquired
// Returns 0 with interrupts preserved if lock was not acquired
bool spinlock_try_lock_until(spinlock_t *lock, uint64_t timeout_time)
{
    cs_enter();

    bool timed_out = false;

    do {
        // Try to immediately acquire the lock
        if (likely(atomic_cmpxchg(lock, 0, 1) == 0))
            return true;

        // Assert if spinning is futile
        assert(spincount_mask);

        while ((*lock != 0) && !(timed_out = (time_ns() >= timeout_time))) {
            // Test many times
            for (size_t spins = 10000; spins; --spins) {
                if (*lock == 0)
                    break;
            }
        }

        // The lock appears to be unlocked...
        // Try again if it looks unlocked, even if it timed out
    } while ((*lock == 0) || !timed_out);

    cs_leave();

    return false;
}

_hot
void spinlock_unlock(spinlock_t *lock)
{
    assert(*lock & 1);
    atomic_st_rel(lock, 0);
    cs_leave();
}

//
// Shared lock. 0 is unlocked, -1 is exclusive, >= 1 is shared lock
// When a writer tries to acquire a lock, it sets bit 30
// to lock out readers

// Expect 0 when acquring, expect 1 when upgrading
static void rwspinlock_ex_lock_impl(rwspinlock_t *lock,
                                    rwspinlock_value_t expect)
{
    bool own_bit30 = false;

    for (rwspinlock_value_t old_value = *lock; ; pause()) {
        if (!own_bit30) {
            //
            // We haven't locked out readers yet

            // Simple scenario, acquire unowned lock
            if (old_value == expect &&
                    atomic_cmpxchg(lock, expect, -1) == expect)
                return;

            // Try to acquire ownership of bit 30
            // If nobody has locked out readers and there is at least one reader
            if (old_value < (1 << 30) && old_value > 0) {
                if (atomic_cmpxchg_upd(lock, &old_value, old_value | (1<<30)))
                    own_bit30 = true;

                continue;
            }

            // Another writer already acquired bit 30...

        } else {
            //
            // We have acquired ownership of bit 30, which
            // locks out readers

            // Wait for readers to drain out and acquire
            // exclusive lock when they have
            if (old_value == (1<<30) + expect) {
                //
                // All (other, when upgrading) readers drained out

                if (atomic_cmpxchg_upd(lock, &old_value, -1))
                    return;

                continue;
            }
        }

        old_value = *lock;
    }
}

void rwspinlock_ex_lock(rwspinlock_t *lock)
{
    rwspinlock_ex_lock_impl(lock, 0);
}

// Upgrade from shared lock to exclusive lock
void rwspinlock_upgrade(rwspinlock_t *lock)
{
    assert(*lock > 0);
    rwspinlock_ex_lock_impl(lock, 1);
}

// Downgrade from exclusive lock to shared lock
void rwspinlock_downgrade(rwspinlock_t *lock)
{
    assert(*lock == -1);
    *lock = 1;
}

void rwspinlock_ex_unlock(rwspinlock_t *lock)
{
    assert(*lock == -1);
    *lock = 0;
}

void rwspinlock_sh_lock(rwspinlock_t *lock)
{
    for (rwspinlock_value_t old_value = *lock; ; pause()) {
        if (old_value >= 0 && old_value < (1<<30)) {
            // It is unlocked or already shared
            // and no writer has acquired bit 30
            // Try to increase shared count
            if (atomic_cmpxchg_upd(lock, &old_value, old_value + 1))
                return;
        } else if (old_value < 0) {
            // Wait for it to not be exclusively held and readers not locked out
            cpu_wait_value(lock, 0,
                           (rwspinlock_value_t(1U << 31) |
                            rwspinlock_value_t(1U << 30)));
            old_value = *lock;
        }
    }
}

void rwspinlock_sh_unlock(rwspinlock_t *lock)
{
    for (rwspinlock_value_t old_value = *lock; ; pause()) {
        if (old_value > 0) {
            // Try to decrease shared count
            if (atomic_cmpxchg_upd(lock, &old_value, old_value - 1))
                break;
        } else {
            // Make sure shared lock is actually held
            assert(old_value > 0);
        }
    }
}

bool rwspinlock_ex_try_lock(rwspinlock_t *lock)
{
    if (*lock == 0)
        return atomic_cmpxchg(lock, 0, -1) == 0;

    return false;
}

bool rwspinlock_sh_try_lock(rwspinlock_t *lock)
{
    for (rwspinlock_value_t expect = *lock; expect >= 0; pause()) {
        if (atomic_cmpxchg_upd(lock, &expect, expect + 1))
            return true;

        // Wait for sign bit to clear
        cpu_wait_value(lock, 0, rwspinlock_value_t(1U << 31));
        expect = *lock;
    }

    return false;
}

void ticketlock_lock(ticketlock_t *lock)
{
    cs_enter();

    ticketlock_value_t my_ticket = atomic_xadd(&lock->next_ticket, 2);

    for (;;) {
        ticketlock_value_t serving = atomic_ld_acq(&lock->now_serving);

        if (likely(my_ticket == (serving & -2))) {
            // Store the interrupt flag in bit 0
            atomic_st_rel(&lock->now_serving, (serving & -2)
                          /* | intr_enabled */);
            return;
        }

        cpu_wait_value(&lock->now_serving, my_ticket);
    }
}

void ticketlock_lock_restore(
        ticketlock_t *lock, ticketlock_value_t saved_lock)
{
    ticketlock_value_t my_ticket = atomic_xadd(&lock->next_ticket, 2);

    for (;;) {
        ticketlock_value_t serving = lock->now_serving;

        if (likely(my_ticket == (serving & -2))) {
            // Store the interrupt flag in bit 0
            atomic_st_rel(&lock->now_serving, (serving & -2) | saved_lock);
            return;
        }

        cpu_wait_value(&lock->now_serving, my_ticket);
    }
}

bool ticketlock_try_lock(ticketlock_t *lock)
{
    cs_enter();

    ticketlock_value_t old_next = lock->next_ticket;
    ticketlock_value_t serving = lock->now_serving;

    if (likely(serving == old_next)) {
        // The lock is available, atomically get a ticket value
        if (likely(atomic_cmpxchg_upd(&lock->next_ticket, &old_next,
                                      (old_next + 2) & -2))) {
            // Got next ticket value

            // Store the interrupt flag in bit 0
            atomic_st_rel(&lock->now_serving,
                          (serving & -2) /*| intr_enabled*/);

            return true;
        }

        // Racing thread got next ticket, give up
    }

    // There are threads queued for it, give up
    cs_leave();
    return false;
}

void ticketlock_unlock(ticketlock_t *lock)
{
    ticketlock_value_t serving = lock->now_serving;
    lock->now_serving = (serving + 2) & -2;
    cs_leave();
}

ticketlock_value_t ticketlock_unlock_save(ticketlock_t *lock)
{
    ticketlock_value_t intr_state = lock->now_serving & 1;
    ticketlock_value_t serving = lock->now_serving;
    lock->now_serving = (serving + 2) & -2;
    return intr_state;
}

bool mcslock_try_lock(mcs_queue_ent_t * volatile * lock,
                             mcs_queue_ent_t *node)
{
    cs_enter();

    atomic_st_rel(&node->next, nullptr);

    if (likely(atomic_cmpxchg(lock, nullptr, node) == nullptr))
        return true;

    cs_leave();
    return false;
}

_hot
void mcslock_lock(mcs_queue_ent_t * volatile *lock,
                         mcs_queue_ent_t *node)
{
    MCSLOCK_TRACE("Acquiring lock @ %p threadid=%d\n",
                  (void*)lock, thread_get_id());

    thread_t tid = thread_get_id();
    node->thread_id = tid;

    cs_enter();

    node->next = nullptr;

    mcs_queue_ent_t *pred = atomic_xchg(lock, node);

    if (unlikely(pred != nullptr)) {
        // queue was non-empty

        atomic_st_rel(&node->locked, true);

        assert(atomic_ld_acq(&pred->thread_id) !=
                atomic_ld_acq(&node->thread_id));

        // Link predecessor to this node
        atomic_st_rel(&pred->next, node);

        // The predecessor will set locked to false when they unlock it
        cpu_wait_value(&node->locked, false);
    }

}

_hot
void mcslock_unlock(mcs_queue_ent_t * volatile *lock,
                    mcs_queue_ent_t *node)
{
    MCSLOCK_TRACE("Releasing lock @ %p threadid=%d\n",
                  (void*)lock, thread_get_id());

    if (atomic_ld_acq(&node->next) == nullptr) {
        // no known successor

        // If the root still points at this node, null it
        if (atomic_cmpxchg(lock, node, nullptr) == node) {
            // List now empty
            cs_leave();
            return;
        }

        // Wait until node->next is not null
        // To be sure successor has initialized locked field
        cpu_wait_not_value(&node->next, (mcs_queue_ent_t*)nullptr);
    }

    atomic_st_rel(&node->next->locked, false);
    cs_leave();
}

