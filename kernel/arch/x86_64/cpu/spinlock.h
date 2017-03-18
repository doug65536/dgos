#pragma once
#include "types.h"

typedef int volatile spinlock_t;

//
// Mutex spinlock

// This is equivalent to spinlock_lock_noirq,
// but for IRQ handlers. Panic if lock is not
// available and there is one CPU.
// (This is would be a bug, any lock acquired
// in an IRQ handler should disable IRQ when
// acquired by the non-IRQ code)
void spinlock_lock_noyield(spinlock_t *lock);

void spinlock_lock(spinlock_t *lock);
int spinlock_try_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);

void spinlock_lock_noirq(spinlock_t *lock);
int spinlock_try_lock_noirq(spinlock_t *lock);
void spinlock_unlock_noirq(spinlock_t *lock);

//
// Reader/writer spinlock

typedef int volatile rwspinlock_t;

void rwspinlock_ex_lock(rwspinlock_t *lock);
void rwspinlock_ex_unlock(rwspinlock_t *lock);

// Upgrade from shared lock to exclusive lock
void rwspinlock_upgrade(rwspinlock_t *lock);

// Downgrade from exclusive lock to shared lock
void rwspinlock_downgrade(rwspinlock_t *lock);

void rwspinlock_sh_lock(rwspinlock_t *lock);
void rwspinlock_sh_unlock(rwspinlock_t *lock);
