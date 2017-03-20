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

spinlock_value_t spinlock_unlock_save(spinlock_t *lock)
{
    assert(*lock != 0);
    return atomic_xchg(lock, 0);
}

void spinlock_lock_restore(spinlock_t *lock, spinlock_value_t saved_lock)
{
    atomic_barrier();
    while (*lock != 0 ||
           atomic_cmpxchg(lock, 0, saved_lock) != 0) {
        // Allow IRQs if they were enabled
        cpu_irq_toggle((saved_lock & 2) != 0);

        pause();

        // Disable IRQs
        cpu_irq_disable();
    }

    // Return with interrupts disabled
}

// Spin to acquire lock, return with IRQs disabled
void spinlock_lock_noirq(spinlock_t *lock)
{
    // Disable IRQs
    int intr_enabled = cpu_irq_disable() << 1;

    atomic_barrier();
    while (*lock != 0 ||
           atomic_cmpxchg(lock, 0, 1 | intr_enabled) != 0) {
        // Allow IRQs if they were enabled
        cpu_irq_toggle(intr_enabled);

        pause();

        // Disable IRQs
        cpu_irq_disable();
    }

    // Return with interrupts disabled
}

// Returns 1 with interrupts disabled if lock was acquired
// Returns 0 with interrupts preserved if lock was not acquired
int spinlock_try_lock_noirq(spinlock_t *lock)
{
    int intr_enabled = cpu_irq_disable() << 1;

    atomic_barrier();
    if (*lock != 0 ||
            atomic_cmpxchg(lock, 0, 1 | intr_enabled) != 0) {
        cpu_irq_toggle(intr_enabled);

        return 0;
    }

    return 1;
}

void spinlock_unlock_noirq(spinlock_t *lock)
{
    int intr_enabled = *lock >> 1;
    assert(*lock & 1);
    *lock = 0;
    cpu_irq_toggle(intr_enabled);
    atomic_barrier();
}

//
// Shared lock. 0 is unlocked, -1 is exclusive, >= 1 is shared lock
// When a writer tries to acquire a lock, it sets bit 30
// to lock out readers

// Expect 0 when acquring, expect 1 when upgrading
static void rwspinlock_ex_lock_impl(rwspinlock_t *lock, int expect)
{
    atomic_barrier();

    int own_bit30 = 0;

    for (rwspinlock_t old_value = *lock; ; pause()) {
        rwspinlock_t upd_value;

        if (!own_bit30) {
            //
            // We haven't locked out readers yet

            // Simple scenario, acquire unowned lock
            if (old_value == expect &&
                    atomic_cmpxchg(lock, expect, -1) == expect)
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
