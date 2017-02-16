#pragma once
#include "types.h"

typedef int volatile spinlock_t;

//
// Mutex spinlock

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
