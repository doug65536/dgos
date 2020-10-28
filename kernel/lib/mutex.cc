#include "mutex.h"
#include "export.h"

#include "cpu/control_regs.h"

#pragma GCC visibility push(default)
template class ext::unique_lock<ext::mutex>;
template class ext::unique_lock<ext::shared_mutex>;

template class ext::unique_lock<ext::shared_spinlock>;
template class ext::unique_lock<ext::ticketlock>;
template class ext::unique_lock<ext::spinlock>;
template class ext::unique_lock<ext::irqlock>;
template class ext::unique_lock<ext::mcslock>;

template class ext::unique_lock<ext::noirq_lock<ext::mutex>>;
template class ext::unique_lock<ext::noirq_lock<ext::shared_mutex>>;

template class ext::unique_lock<ext::noirq_lock<ext::shared_spinlock>>;
template class ext::unique_lock<ext::irq_ticketlock>;
template class ext::unique_lock<ext::irq_spinlock>;
template class ext::unique_lock<ext::noirq_lock<ext::irqlock>>;
//template class ext::unique_lock<ext::noirq_lock<ext::mcslock>>;
#pragma GCC visibility pop

ext::mutex::mutex()
{
    mutex_init(&m);
}

ext::mutex::~mutex()
{
    mutex_destroy(&m);
}

void ext::mutex::lock()
{
    mutex_lock(&m);
}

bool ext::mutex::try_lock()
{
    return mutex_try_lock(&m);
}

void ext::mutex::unlock()
{
    mutex_unlock(&m);
}

ext::shared_mutex::shared_mutex()
{
    rwlock_init(&m);
}

ext::shared_mutex::~shared_mutex()
{
    rwlock_destroy(&m);
}

void ext::shared_mutex::lock()
{
    rwlock_ex_lock(&m);
}

bool ext::shared_mutex::try_lock()
{
    return rwlock_ex_try_lock(&m);
}

void ext::shared_mutex::unlock()
{
    rwlock_ex_unlock(&m);
}

void ext::shared_mutex::lock_shared()
{
    rwlock_sh_lock(&m);
}

void ext::shared_mutex::try_lock_shared()
{
    rwlock_sh_try_lock(&m);
}

void ext::shared_mutex::unlock_shared()
{
    rwlock_sh_unlock(&m);
}

void ext::shared_mutex::upgrade_lock()
{
    rwlock_upgrade(&m);
}

// ---

ext::unique_lock<ext::mcslock>::unique_lock(ext::mcslock &attached_lock)
    : m(&attached_lock)
    , locked(false)
{
    lock();
}

ext::unique_lock<ext::mcslock>::unique_lock(
        ext::mcslock &lock, ext::defer_lock_t) noexcept
    : m(&lock)
    , locked(false)
{
}

ext::unique_lock<ext::mcslock>::~unique_lock() noexcept
{
    unlock();
}

void ext::unique_lock<ext::mcslock>::lock() noexcept
{
    assert(!locked);
    m->lock(&node);
    locked = true;
}

void ext::unique_lock<ext::mcslock>::unlock() noexcept
{
    if (locked) {
        locked = false;
        m->unlock(&node);
    }
}

void ext::unique_lock<ext::mcslock>::release() noexcept
{
    locked = false;
    m = nullptr;
}

void ext::unique_lock<ext::mcslock>::swap(
        unique_lock<ext::mcslock> &rhs) noexcept
{
    ext::swap(rhs.m, m);
    ext::swap(rhs.locked, locked);
}


// ---

KERNEL_API ext::condition_variable::condition_variable()
{
    condvar_init(&m);
}

ext::condition_variable::~condition_variable()
{
    condvar_destroy(&m);
}

void ext::condition_variable::notify_one()
{
    condvar_wake_one(&m);
}

void ext::condition_variable::notify_all()
{
    condvar_wake_all(&m);
}

void ext::condition_variable::notify_n(size_t n)
{
    condvar_wake_n(&m, n);
}

void ext::spinlock::lock()
{
    spinlock_lock(&m);
}

bool ext::spinlock::try_lock()
{
    return spinlock_try_lock(&m);
}

void ext::spinlock::unlock()
{
    spinlock_unlock(&m);
}

spinlock_t &ext::spinlock::native_handle()
{
    return m;
}

void ext::shared_spinlock::lock()
{
    rwspinlock_ex_lock(&m);
}

bool ext::shared_spinlock::try_lock()
{
    return rwspinlock_ex_try_lock(&m);
}

void ext::shared_spinlock::unlock()
{
    rwspinlock_ex_unlock(&m);
}

void ext::shared_spinlock::lock_shared()
{
    rwspinlock_sh_lock(&m);
}

bool ext::shared_spinlock::try_lock_shared()
{
    return rwspinlock_sh_try_lock(&m);
}

void ext::shared_spinlock::unlock_shared()
{
    rwspinlock_sh_unlock(&m);
}

void ext::shared_spinlock::upgrade_lock()
{
    rwspinlock_upgrade(&m);
}

ext::shared_spinlock::mutex_type &ext::shared_spinlock::native_handle()
{
    return m;
}

ext::spinlock::~spinlock()
{
    assert(m == 0);
}

void ext::ticketlock::lock()
{
    ticketlock_lock(&m);
}

bool ext::ticketlock::try_lock()
{
    return ticketlock_try_lock(&m);
}

void ext::ticketlock::unlock()
{
    ticketlock_unlock(&m);
}

ticketlock_t &ext::ticketlock::native_handle()
{
    return m;
}




ext::mcslock::~mcslock()
{
    assert(m == nullptr);
}

void ext::mcslock::lock(mcs_queue_ent_t *node)
{
    mcslock_lock(&m, node);
}

bool ext::mcslock::try_lock(mcs_queue_ent_t *node)
{
    return mcslock_try_lock(&m, node);
}

void ext::mcslock::unlock(mcs_queue_ent_t *node)
{
    mcslock_unlock(&m, node);
}

ext::irqlock::~irqlock()
{
    if (saved_mask > 0)
        cpu_irq_enable();
    saved_mask = 0;
}

void ext::irqlock::lock()
{
    assert(saved_mask == 0);
    // Gets 2 or 0, then subtracts one, making it either 1 or -1
    saved_mask = (cpu_irq_save_disable() << 1) - 1;
}

void ext::irqlock::unlock()
{
    assert(saved_mask != 0);
    cpu_irq_toggle(saved_mask > 0);
}

int &ext::irqlock::native_handle()
{
    return saved_mask;
}
