#pragma once
#include "types.h"

__BEGIN_DECLS

//
// Mutex spinlock

typedef int spinlock_value_t;
typedef spinlock_value_t volatile spinlock_t;

KERNEL_API void spinlock_lock(spinlock_t *lock);
KERNEL_API bool spinlock_try_lock(spinlock_t *lock);
KERNEL_API bool spinlock_try_lock_until(spinlock_t *lock, uint64_t timeout_time);
KERNEL_API void spinlock_unlock(spinlock_t *lock);

//
// Reader/writer spinlock

typedef int rwspinlock_value_t;
typedef rwspinlock_value_t volatile rwspinlock_t;

KERNEL_API void rwspinlock_ex_lock(rwspinlock_t *lock);
KERNEL_API void rwspinlock_ex_unlock(rwspinlock_t *lock);

KERNEL_API bool rwspinlock_ex_try_lock(rwspinlock_t *lock);
KERNEL_API bool rwspinlock_sh_try_lock(rwspinlock_t *lock);

// Upgrade from shared lock to exclusive lock
KERNEL_API void rwspinlock_upgrade(rwspinlock_t *lock);

// Downgrade from exclusive lock to shared lock
KERNEL_API void rwspinlock_downgrade(rwspinlock_t *lock);

KERNEL_API void rwspinlock_sh_lock(rwspinlock_t *lock);
KERNEL_API void rwspinlock_sh_unlock(rwspinlock_t *lock);

typedef unsigned ticketlock_value_t;

struct ticketlock_t {
    constexpr ticketlock_t()
        : now_serving(0)
        , next_ticket(0)
    {
    }

    // Bit 0 holds the saved interrupt flag
    ticketlock_value_t volatile now_serving;

    // This is always advanced by 2, so bit 0 is always 0
    ticketlock_value_t volatile next_ticket;
};

KERNEL_API void ticketlock_lock(ticketlock_t *lock);
KERNEL_API bool ticketlock_try_lock(ticketlock_t *lock);
KERNEL_API void ticketlock_unlock(ticketlock_t *lock);
KERNEL_API ticketlock_value_t ticketlock_unlock_save(ticketlock_t *lock);
KERNEL_API void ticketlock_lock_restore(ticketlock_t *lock,
                                        ticketlock_value_t saved_lock);

struct mcs_queue_ent_t {
    mcs_queue_ent_t * volatile next;
    int thread_id;
    bool volatile locked;
};

KERNEL_API void mcslock_lock(mcs_queue_ent_t * volatile *lock,
                             mcs_queue_ent_t *node);
KERNEL_API bool mcslock_try_lock(mcs_queue_ent_t * volatile *lock,
                                 mcs_queue_ent_t *node);
KERNEL_API void mcslock_unlock(mcs_queue_ent_t * volatile *lock,
                               mcs_queue_ent_t *node);

__END_DECLS
