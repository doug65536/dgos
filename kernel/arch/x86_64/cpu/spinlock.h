#pragma once
#include "types.h"

typedef int volatile spinlock_t;

//
// Mutex spinlock

void spinlock_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);

//
// Reader/writer spinlock

typedef int volatile rwspinlock_t;

void rwspinlock_ex_lock(rwspinlock_t *lock);
void rwspinlock_ex_unlock(rwspinlock_t *lock);

void rwspinlock_sh_lock(rwspinlock_t *lock);
void rwspinlock_sh_unlock(rwspinlock_t *lock);
