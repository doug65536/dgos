#pragma once
#include "types.h"

typedef int volatile spinlock_t;

//
// Mutex spinlock

void spinlock_lock(spinlock_t *lock);
int spinlock_try_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);

typedef struct spinlock_hold_t {
    int intr_enabled;
} spinlock_hold_t;

spinlock_hold_t spinlock_lock_noirq(spinlock_t *lock);
int spinlock_try_lock_noirq(spinlock_t *lock, spinlock_hold_t *hold);
void spinlock_unlock_noirq(spinlock_t *lock, spinlock_hold_t *hold);

//
// Reader/writer spinlock

typedef int volatile rwspinlock_t;

void rwspinlock_ex_lock(rwspinlock_t *lock);
void rwspinlock_ex_unlock(rwspinlock_t *lock);

void rwspinlock_sh_lock(rwspinlock_t *lock);
void rwspinlock_sh_unlock(rwspinlock_t *lock);
