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
    : magic(expected_magic)
{
    mutex_init(&m);
}

ext::mutex::~mutex()
{
    assert(magic == expected_magic);
    mutex_destroy(&m);
    magic = 0;
    __asm__("":::"memory");
}

void ext::mutex::lock()
{
    assert(magic == expected_magic);
    mutex_lock(&m);
}

bool ext::mutex::try_lock()
{
    assert(magic == expected_magic);
    return mutex_try_lock(&m);
}

void ext::mutex::unlock()
{
    assert(magic == expected_magic);
    mutex_unlock(&m);
}

ext::shared_mutex::shared_mutex()
    : magic(expected_magic)
{
    rwlock_init(&m);
}

ext::shared_mutex::~shared_mutex()
{
    assert(magic == expected_magic);
    rwlock_destroy(&m);
    magic = 0;
    __asm__("":::"memory");
}

void ext::shared_mutex::lock()
{
    assert(magic == expected_magic);
    rwlock_ex_lock(&m);
}

bool ext::shared_mutex::try_lock()
{
    assert(magic == expected_magic);
    return rwlock_ex_try_lock(&m);
}

void ext::shared_mutex::unlock()
{
    assert(magic == expected_magic);
    rwlock_ex_unlock(&m);
}

void ext::shared_mutex::lock_shared()
{
    assert(magic == expected_magic);
    rwlock_sh_lock(&m);
}

void ext::shared_mutex::try_lock_shared()
{
    assert(magic == expected_magic);
    rwlock_sh_try_lock(&m);
}

void ext::shared_mutex::unlock_shared()
{
    assert(magic == expected_magic);
    rwlock_sh_unlock(&m);
}

void ext::shared_mutex::upgrade_lock()
{
    assert(magic == expected_magic);
    rwlock_upgrade(&m);
}

// ---

ext::unique_lock<ext::mcslock>::unique_lock(ext::mcslock &attached_lock)
    : magic(expected_magic)
    , m(&attached_lock)
    , locked(false)
{
    lock();
}

ext::unique_lock<ext::mcslock>::unique_lock(
        ext::mcslock &lock, ext::defer_lock_t) noexcept
    : magic(expected_magic)
    , m(&lock)
    , locked(false)
{
}

ext::unique_lock<ext::mcslock>::~unique_lock() noexcept
{
    assert(magic == expected_magic);
    unlock();
    magic = 0;
    __asm__("":::"memory");
}

void ext::unique_lock<ext::mcslock>::lock() noexcept
{
    assert(magic == expected_magic);
    assert(!locked);
    m->lock(&node);
    locked = true;
}

void ext::unique_lock<ext::mcslock>::unlock() noexcept
{
    assert(magic == expected_magic);
    if (locked) {
        locked = false;
        m->unlock(&node);
    }
}

void ext::unique_lock<ext::mcslock>::release() noexcept
{
    assert(magic == expected_magic);
    locked = false;
    m = nullptr;
}

void ext::unique_lock<ext::mcslock>::swap(
        unique_lock<ext::mcslock> &rhs) noexcept
{
    assert(magic == expected_magic);
    assert(rhs.magic == expected_magic);
    ext::swap(rhs.m, m);
    ext::swap(rhs.locked, locked);
}


// ---

ext::condition_variable::condition_variable()
    : magic(expected_magic)
{
    condvar_init(&m);
}

ext::condition_variable::~condition_variable()
{
    assert(magic == expected_magic);
    condvar_destroy(&m);
    magic = 0;
    __asm__("":::"memory");
}

void ext::condition_variable::notify_one()
{
    assert(magic == expected_magic);
    condvar_wake_one(&m);
}

void ext::condition_variable::notify_all()
{
    assert(magic == expected_magic);
    condvar_wake_all(&m);
}

void ext::condition_variable::notify_n(size_t n)
{
    assert(magic == expected_magic);
    condvar_wake_n(&m, n);
}

void ext::spinlock::lock()
{
    assert(magic == expected_magic);
    spinlock_lock(&m);
}

bool ext::spinlock::try_lock()
{
    assert(magic == expected_magic);
    return spinlock_try_lock(&m);
}

void ext::spinlock::unlock()
{
    assert(magic == expected_magic);
    spinlock_unlock(&m);
}

spinlock_t &ext::spinlock::native_handle()
{
    assert(magic == expected_magic);
    return m;
}

void ext::shared_spinlock::lock()
{
    assert(magic == expected_magic);
    rwspinlock_ex_lock(&m);
}

bool ext::shared_spinlock::try_lock()
{
    assert(magic == expected_magic);
    return rwspinlock_ex_try_lock(&m);
}

void ext::shared_spinlock::unlock()
{
    assert(magic == expected_magic);
    rwspinlock_ex_unlock(&m);
}

void ext::shared_spinlock::lock_shared()
{
    assert(magic == expected_magic);
    rwspinlock_sh_lock(&m);
}

bool ext::shared_spinlock::try_lock_shared()
{
    assert(magic == expected_magic);
    return rwspinlock_sh_try_lock(&m);
}

void ext::shared_spinlock::unlock_shared()
{
    assert(magic == expected_magic);
    rwspinlock_sh_unlock(&m);
}

void ext::shared_spinlock::upgrade_lock()
{
    assert(magic == expected_magic);
    rwspinlock_upgrade(&m);
}

ext::shared_spinlock::mutex_type &ext::shared_spinlock::native_handle()
{
    assert(magic == expected_magic);
    return m;
}

ext::spinlock::~spinlock()
{
    assert(magic == expected_magic);
    assert(m == 0);
    magic = 0;
    __asm__("":::"memory");
}

void ext::ticketlock::lock()
{
    assert(magic == expected_magic);
    ticketlock_lock(&m);
}

bool ext::ticketlock::try_lock()
{
    assert(magic == expected_magic);
    return ticketlock_try_lock(&m);
}

void ext::ticketlock::unlock()
{
    assert(magic == expected_magic);
    ticketlock_unlock(&m);
}

ticketlock_t &ext::ticketlock::native_handle()
{
    assert(magic == expected_magic);
    return m;
}

ext::mcslock::~mcslock()
{
    assert(magic == expected_magic);
    assert(m == nullptr);
}

void ext::mcslock::lock(mcs_queue_ent_t *node)
{
    assert(magic == expected_magic);
    mcslock_lock(&m, node);
}

bool ext::mcslock::try_lock(mcs_queue_ent_t *node)
{
    assert(magic == expected_magic);
    return mcslock_try_lock(&m, node);
}

void ext::mcslock::unlock(mcs_queue_ent_t *node)
{
    assert(magic == expected_magic);
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
