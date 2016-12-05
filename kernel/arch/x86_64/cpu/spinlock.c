#include "spinlock.h"
#include "atomic.h"
#include "assert.h"

//
// Exclusive lock. 0 is unlocked, 1 is locked

void spinlock_lock(spinlock_t *lock)
{
    while (*lock != 0 || atomic_cmpxchg(lock, 0, 1) != 0)
        pause();
}

void spinlock_unlock(spinlock_t *lock)
{
    assert(*lock != 0);
    *lock = 0;
}

//
// Shared lock. 0 is unlocked, -1 is exclusive, >= 1 is shared lock

void rwspinlock_ex_lock(rwspinlock_t *lock)
{
    while (*lock != 0 || atomic_cmpxchg(lock, 0, -1) != 0)
        pause();
}

void rwspinlock_ex_unlock(rwspinlock_t *lock)
{
    assert(*lock == -1);
    *lock = 0;
}

void rwspinlock_sh_lock(rwspinlock_t *lock)
{
    rwspinlock_t old_value = *lock;
    for (;;) {
        if (old_value >= 0) {
            // It is unlocked or already shared
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
        }
    }
}

void rwspinlock_sh_unlock(rwspinlock_t *lock)
{
    rwspinlock_t old_value = *lock;
    for (;;) {
        if (old_value > 0) {
            // Try to decrease shared count
            rwspinlock_t new_value = old_value - 1;
            rwspinlock_t cur_value = atomic_cmpxchg(
                        lock, old_value, new_value);

            if (cur_value == old_value)
                break;

            old_value = cur_value;
            pause();
        } else {
            // Make sure shared lock is actually held
            assert(old_value > 0);
        }
    }
}
