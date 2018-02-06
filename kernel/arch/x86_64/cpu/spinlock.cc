#include "spinlock.h"
#include "atomic.h"
#include "assert.h"
#include "control_regs.h"
#include "thread.h"
#include "printk.h"

//
// Exclusive lock. 0 is unlocked, 1 is locked

void spinlock_lock_noyield(spinlock_t *lock)
{
    // Disable IRQs
    int intr_enabled = cpu_irq_disable() << 1;

    atomic_barrier();
    while (*lock != 0 ||
           atomic_cmpxchg(lock, 0, 1 | intr_enabled) != 0) {
        // Allow IRQs if they were enabled
        cpu_irq_toggle(intr_enabled);

        if (likely(spincount_mask))
            pause();
        else
            panic("Deadlock acquiring spinlock in IRQ handler!");

        // Disable IRQs
        cpu_irq_disable();
    }

    // Return with interrupts disabled
}

void spinlock_lock(spinlock_t *lock)
{
    atomic_barrier();
    while (*lock != 0 || atomic_cmpxchg(lock, 0, 1) != 0) {
        if (spincount_mask)
            pause();
        else
            thread_yield();
    }
}

bool spinlock_try_lock(spinlock_t *lock)
{
    atomic_barrier();
    if (*lock != 0 || atomic_cmpxchg(lock, 0, 1) != 0)
        return false;
    return true;
}

void spinlock_unlock(spinlock_t *lock)
{
    assert(*lock != 0);
    *lock = 0;
}

spinlock_value_t spinlock_unlock_save(spinlock_t *lock)
{
    assert(*lock != 0);
    return atomic_xchg(lock, 0);
}

void spinlock_lock_restore(spinlock_t *lock, spinlock_value_t saved_lock)
{
    while (*lock != 0 || atomic_cmpxchg(lock, 0, saved_lock) != 0)
        pause();
}

// Spin to acquire lock, return with IRQs disabled
void spinlock_lock_noirq(spinlock_t *lock)
{
    // Disable IRQs
    int intr_enabled = cpu_irq_disable() << 1;

    if (intr_enabled) {
        while (*lock != 0 || atomic_cmpxchg(lock, 0, 1 | intr_enabled) != 0) {
            // Allow IRQs if they were enabled
            cpu_irq_enable();

            pause();

            // Disable IRQs
            cpu_irq_disable();
        }
    } else {
        while (*lock != 0 ||
               atomic_cmpxchg(lock, 0, 1 | intr_enabled) != 0)
            pause();
    }

    // Return with interrupts disabled
}

// Returns 1 with interrupts disabled if lock was acquired
// Returns 0 with interrupts preserved if lock was not acquired
bool spinlock_try_lock_noirq(spinlock_t *lock)
{
    int intr_enabled = cpu_irq_disable() << 1;

    if (*lock != 0 || atomic_cmpxchg(lock, 0, 1 | intr_enabled) != 0) {
        cpu_irq_toggle(intr_enabled);

        return false;
    }

    return true;
}

void spinlock_unlock_noirq(spinlock_t *lock)
{
    int intr_enabled = *lock >> 1;
    assert(*lock & 1);
    *lock = 0;
    cpu_irq_toggle(intr_enabled);
}

//
// Shared lock. 0 is unlocked, -1 is exclusive, >= 1 is shared lock
// When a writer tries to acquire a lock, it sets bit 30
// to lock out readers

// Expect 0 when acquring, expect 1 when upgrading
static void rwspinlock_ex_lock_impl(rwspinlock_t *lock,
                                    rwspinlock_value_t expect)
{
    atomic_barrier();

    bool own_bit30 = false;

    for (rwspinlock_value_t old_value = *lock; ; pause()) {
        if (!own_bit30) {
            //
            // We haven't locked out readers yet

            // Simple scenario, acquire unowned lock
            if (old_value == expect &&
                    atomic_cmpxchg(lock, expect, -1) == expect)
                break;

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
                    break;

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
                break;
        } else if (old_value < 0) {
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
    }

    return false;
}

void ticketlock_lock(ticketlock_t *lock)
{
    unsigned my_ticket = atomic_xadd(&lock->next_ticket, 1);

    for (;;) {
        unsigned pause_count = my_ticket - lock->now_serving;

        if (pause_count == 0)
            return;

        while (pause_count--)
            pause();
    }
}

bool ticketlock_try_lock(ticketlock_t *lock)
{
    ticketlock_value_t intr_enabled = cpu_irq_disable() << 31;

    for (unsigned old_next = lock->next_ticket; ; ) {
        if (lock->now_serving == old_next) {
            if (atomic_cmpxchg_upd(&lock->next_ticket, &old_next,
                                   ((old_next + 1) & 0x7FFFFFFF) |
                                   intr_enabled))
                return true;
        } else {
            cpu_irq_toggle(intr_enabled);
            return false;
        }
    }
}

void ticketlock_unlock(ticketlock_t *lock)
{
    ++lock->now_serving;
}
