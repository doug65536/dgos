#include "spinlock.h"
#include "atomic.h"
#include "assert.h"
#include "control_regs.h"

//
// Exclusive lock. 0 is unlocked, 1 is locked

void spinlock_lock(spinlock_t *lock)
{
    atomic_barrier();
    while (*lock != 0 || atomic_cmpxchg(lock, 0, 1) != 0)
        pause();
}

int spinlock_try_lock(spinlock_t *lock)
{
    atomic_barrier();
    if (*lock != 0 || atomic_cmpxchg(lock, 0, 1) != 0)
        return 0;
    return 1;
}

void spinlock_unlock(spinlock_t *lock)
{
    assert(*lock != 0);
    *lock = 0;
    atomic_barrier();
}

// Spin to acquire lock, return with IRQs disable
spinlock_hold_t spinlock_lock_noirq(spinlock_t *lock)
{
    spinlock_hold_t hold;

    // Disable IRQs
    hold.intr_enabled = cpu_irq_disable();

    atomic_barrier();
    while (*lock != 0 || atomic_cmpxchg(lock, 0, 1) != 0) {
        // Allow IRQs if they were enabled
        cpu_irq_toggle(hold.intr_enabled);

        pause();

        // Disable IRQs
        cpu_irq_disable();
    }

    // Return with interrupts disabled
    return hold;
}

// Returns 1 with interrupts disabled if lock was acquired
// Returns 0 with interrupts preserved if lock was not acquired
int spinlock_try_lock_noirq(spinlock_t *lock, spinlock_hold_t *hold)
{
    hold->intr_enabled = cpu_irq_disable();

    atomic_barrier();
    if (*lock != 0 || atomic_cmpxchg(lock, 0, 1) != 0) {
        cpu_irq_toggle(hold->intr_enabled);

        return 0;
    }

    return 1;
}

void spinlock_unlock_noirq(spinlock_t *lock, spinlock_hold_t *hold)
{
    assert(*lock != 0);
    *lock = 0;
    cpu_irq_toggle(hold->intr_enabled);
    atomic_barrier();
}

//
// Shared lock. 0 is unlocked, -1 is exclusive, >= 1 is shared lock
// When a writer tries to acquire a lock, it sets bit 30
// to lock out readers

void rwspinlock_ex_lock(rwspinlock_t *lock)
{
    atomic_barrier();

    int own_bit30 = 0;

    for (rwspinlock_t old_value = *lock; ; pause()) {
        rwspinlock_t upd_value;

        if (!own_bit30) {
            //
            // We haven't locked out readers yet

            // Simple scenario, acquire unowned lock
            if (old_value == 0 && atomic_cmpxchg(lock, 0, -1) == 0)
                break;

            // Try to acquire ownership of bit 30
            if (old_value < (1 << 30) && old_value > 0) {
                upd_value = atomic_cmpxchg(
                        lock, old_value, old_value | (1<<30));

                if (upd_value == old_value)
                    own_bit30 = 1;

                old_value = upd_value;
                continue;
            }

            // Another writer already acquired bit 30...

        } else {
            //
            // We have acquired ownership of bit 30, which
            // locks out readers

            // Wait for readers to drain out and acquire
            // exclusive lock when they have
            if (old_value == (1<<30)) {
                //
                // All readers drained out

                upd_value = atomic_cmpxchg(
                        lock, old_value, -1);

                if (upd_value == old_value)
                    break;

                old_value = upd_value;
                continue;
            }
        }

        old_value = *lock;
    }
}

void rwspinlock_ex_unlock(rwspinlock_t *lock)
{
    assert(*lock == -1);
    *lock = 0;
    atomic_barrier();
}

void rwspinlock_sh_lock(rwspinlock_t *lock)
{
    atomic_barrier();
    rwspinlock_t old_value = *lock;
    for (;;) {
        if (old_value >= 0 && old_value < (1<<30)) {
            // It is unlocked or already shared
            // and no writer has acquired bit 30
            // Try to increase shared count
            rwspinlock_t new_value = old_value + 1;
            rwspinlock_t cur_value = atomic_cmpxchg(
                        lock, old_value, new_value);

            if (cur_value == old_value)
                break;

            old_value = cur_value;
            pause();
        } else if (old_value < 0) {
            // It is exclusive, go into reading-only
            // loop to allow shared cache line
            do {
                pause();
                old_value = *lock;
            } while (old_value < 0);
        } else {
            pause();
        }
    }
}

void rwspinlock_sh_unlock(rwspinlock_t *lock)
{
    atomic_barrier();
    rwspinlock_t old_value = *lock;
    for (;; pause()) {
        if (old_value > 0) {
            // Try to decrease shared count
            rwspinlock_t cur_value = atomic_cmpxchg(
                        lock, old_value, old_value - 1);

            if (cur_value == old_value)
                break;

            old_value = cur_value;
        } else {
            // Make sure shared lock is actually held
            assert(old_value > 0);
        }
    }
}
