#pragma once
#include "types.h"

typedef int spinlock_value_t;
typedef spinlock_value_t volatile spinlock_t;

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
bool spinlock_try_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);

void spinlock_lock_noirq(spinlock_t *lock);
bool spinlock_try_lock_noirq(spinlock_t *lock);
void spinlock_unlock_noirq(spinlock_t *lock);

spinlock_value_t spinlock_unlock_save(spinlock_t *lock);
void spinlock_lock_restore(spinlock_t *lock, spinlock_value_t saved_lock);

//
// Reader/writer spinlock

typedef int rwspinlock_value_t;
typedef rwspinlock_value_t volatile rwspinlock_t;

void rwspinlock_ex_lock(rwspinlock_t *lock);
void rwspinlock_ex_unlock(rwspinlock_t *lock);

bool rwspinlock_ex_try_lock(rwspinlock_t *lock);
bool rwspinlock_sh_try_lock(rwspinlock_t *lock);

// Upgrade from shared lock to exclusive lock
void rwspinlock_upgrade(rwspinlock_t *lock);

// Downgrade from exclusive lock to shared lock
void rwspinlock_downgrade(rwspinlock_t *lock);

void rwspinlock_sh_lock(rwspinlock_t *lock);
void rwspinlock_sh_unlock(rwspinlock_t *lock);

typedef unsigned ticketlock_value_t;
struct ticketlock_t {
    // Bit 0 holds the saved interrupt flag
    ticketlock_value_t volatile now_serving;

    // This is always advanced by 2, so bit 0 is always 0
    ticketlock_value_t volatile next_ticket;
};

void ticketlock_lock(ticketlock_t *lock);
bool ticketlock_try_lock(ticketlock_t *lock);
void ticketlock_unlock(ticketlock_t *lock);

struct mcs_queue_ent_t {
    mcs_queue_ent_t * volatile next;
    bool volatile locked;
    bool irq_enabled;
};

void mcslock_lock(mcs_queue_ent_t **lock, mcs_queue_ent_t *node);
bool mcslock_try_lock(mcs_queue_ent_t **lock, mcs_queue_ent_t *node);
void mcslock_unlock(mcs_queue_ent_t **lock, mcs_queue_ent_t *node);
