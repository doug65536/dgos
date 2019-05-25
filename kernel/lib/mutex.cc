#include "mutex.h"
#include "export.h"

#pragma GCC visibility push(default)
template class std::unique_lock<std::mutex>;
template class std::unique_lock<std::shared_mutex>;
template class std::unique_lock<ext::mcslock>;
template class std::unique_lock<ext::spinlock>;
template class std::unique_lock<ext::ticketlock>;
template class std::unique_lock<ext::shared_spinlock>;
#pragma GCC visibility pop

EXPORT std::mutex::mutex()
{
    mutex_init(&m);
}

EXPORT std::mutex::~mutex()
{
    mutex_destroy(&m);
}

EXPORT void std::mutex::lock()
{
    mutex_lock(&m);
}

EXPORT bool std::mutex::try_lock()
{
    return mutex_try_lock(&m);
}

EXPORT void std::mutex::unlock()
{
    mutex_unlock(&m);
}

EXPORT std::shared_mutex::shared_mutex()
{
    rwlock_init(&m);
}

EXPORT std::shared_mutex::~shared_mutex()
{
    rwlock_destroy(&m);
}

EXPORT void std::shared_mutex::lock()
{
    rwlock_ex_lock(&m);
}

EXPORT bool std::shared_mutex::try_lock()
{
    return rwlock_ex_try_lock(&m);
}

EXPORT void std::shared_mutex::unlock()
{
    rwlock_ex_unlock(&m);
}

EXPORT void std::shared_mutex::lock_shared()
{
    rwlock_sh_lock(&m);
}

EXPORT void std::shared_mutex::try_lock_shared()
{
    rwlock_sh_try_lock(&m);
}

EXPORT void std::shared_mutex::unlock_shared()
{
    rwlock_sh_unlock(&m);
}

EXPORT void std::shared_mutex::upgrade_lock()
{
    rwlock_upgrade(&m);
}

EXPORT std::unique_lock<ext::mcslock>::unique_lock(ext::mcslock &attached_lock)
    : m(&attached_lock)
    , locked(false)
{
    lock();
}

EXPORT std::unique_lock<ext::mcslock>::unique_lock(
        ext::mcslock &lock, defer_lock_t) noexcept
    : m(&lock)
    , locked(false)
{
}

EXPORT std::unique_lock<ext::mcslock>::~unique_lock()
{
    unlock();
}

EXPORT void std::unique_lock<ext::mcslock>::lock() noexcept
{
    assert(!locked);
    m->lock(&node);
    locked = true;
}

EXPORT void std::unique_lock<ext::mcslock>::unlock() noexcept
{
    if (locked) {
        locked = false;
        m->unlock(&node);
    }
}

EXPORT void std::unique_lock<ext::mcslock>::release() noexcept
{
    locked = false;
    m = nullptr;
}

EXPORT void std::unique_lock<ext::mcslock>::swap(
        unique_lock<ext::mcslock> &rhs) noexcept
{
    std::swap(rhs.m, m);
    std::swap(rhs.locked, locked);
}

EXPORT std::condition_variable::condition_variable()
{
    condvar_init(&m);
}

EXPORT std::condition_variable::~condition_variable()
{
    condvar_destroy(&m);
}

EXPORT void std::condition_variable::notify_one()
{
    condvar_wake_one(&m);
}

EXPORT void std::condition_variable::notify_all()
{
    condvar_wake_all(&m);
}

EXPORT void std::condition_variable::notify_n(size_t n)
{
    condvar_wake_n(&m, n);
}

EXPORT void std::condition_variable::wait(std::unique_lock<mutex> &lock)
{
    assert(lock.is_locked());
    condvar_wait(&m, &lock.native_handle());
    assert(lock.is_locked());
}

EXPORT void std::condition_variable::wait(std::unique_lock<ext::spinlock> &lock)
{
    assert(lock.is_locked());
    condvar_wait_spinlock(&m, &lock.native_handle());
    assert(lock.is_locked());
}

EXPORT void std::condition_variable::wait(
        std::unique_lock<ext::ticketlock> &lock)
{
    assert(lock.is_locked());
    condvar_wait_ticketlock(&m, &lock.native_handle(), uint64_t(-1));
    assert(lock.is_locked());
}

EXPORT void std::condition_variable::wait(std::unique_lock<ext::mcslock> &lock)
{
    assert(lock.is_locked());
    condvar_wait_mcslock(&m, &lock.native_handle(), &lock.wait_node());
    assert(lock.is_locked());
}

EXPORT std::cv_status std::condition_variable::wait_until(
        std::unique_lock<std::mutex>& lock, int64_t timeout_time)
{
    assert(lock.is_locked());

    bool result = condvar_wait(
                &m, &lock.native_handle(), timeout_time);

    assert(lock.is_locked());

    return result ? std::cv_status::no_timeout : std::cv_status::timeout;
}

EXPORT std::cv_status std::condition_variable::wait_until(
        std::unique_lock<ext::spinlock>& lock, int64_t timeout_time)
{
    assert(lock.is_locked());

    bool result = condvar_wait_spinlock(
                &m, &lock.native_handle(), timeout_time);

    assert(lock.is_locked());

    return result ? std::cv_status::no_timeout : std::cv_status::timeout;
}

EXPORT std::cv_status std::condition_variable::wait_until(
        std::unique_lock<ext::ticketlock>& lock, int64_t timeout_time)
{
    assert(lock.is_locked());

    bool result = condvar_wait_ticketlock(
                &m, &lock.native_handle(), timeout_time);

    assert(lock.is_locked());

    return result ? std::cv_status::no_timeout : std::cv_status::timeout;
}

EXPORT std::cv_status std::condition_variable::wait_until(
        std::unique_lock<ext::mcslock>& lock, int64_t timeout_time)
{
    assert(lock.is_locked());

    bool result = condvar_wait_mcslock(
                &m, &lock.native_handle(), &lock.wait_node(), timeout_time);

    assert(lock.is_locked());

    return result ? std::cv_status::no_timeout : std::cv_status::timeout;
}

EXPORT void ext::spinlock::lock()
{
    spinlock_lock(&m);
}

EXPORT bool ext::spinlock::try_lock()
{
    return spinlock_try_lock(&m);
}

EXPORT void ext::spinlock::unlock()
{
    spinlock_unlock(&m);
}

EXPORT spinlock_t &ext::spinlock::native_handle()
{
    return m;
}

EXPORT void ext::shared_spinlock::lock()
{
    rwspinlock_ex_lock(&m);
}

EXPORT bool ext::shared_spinlock::try_lock()
{
    return rwspinlock_ex_try_lock(&m);
}

EXPORT void ext::shared_spinlock::unlock()
{
    rwspinlock_ex_unlock(&m);
}

EXPORT void ext::shared_spinlock::lock_shared()
{
    rwspinlock_sh_lock(&m);
}

EXPORT void ext::shared_spinlock::try_lock_shared()
{
    rwspinlock_sh_try_lock(&m);
}

EXPORT void ext::shared_spinlock::unlock_shared()
{
    rwspinlock_sh_unlock(&m);
}

EXPORT void ext::shared_spinlock::upgrade_lock()
{
    rwspinlock_upgrade(&m);
}

EXPORT ext::shared_spinlock::mutex_type &ext::shared_spinlock::native_handle()
{
    return m;
}

EXPORT ext::spinlock::~spinlock()
{
    assert(m == 0);
}

EXPORT void ext::ticketlock::lock()
{
    ticketlock_lock(&m);
}

EXPORT bool ext::ticketlock::try_lock()
{
    return ticketlock_try_lock(&m);
}

EXPORT void ext::ticketlock::unlock()
{
    ticketlock_unlock(&m);
}

EXPORT ticketlock_t &ext::ticketlock::native_handle()
{
    return m;
}

EXPORT ext::mcslock::mcslock()
    : m(nullptr)
{
}

EXPORT ext::mcslock::~mcslock()
{
    assert(m == nullptr);
}

EXPORT void ext::mcslock::lock(mcs_queue_ent_t *node)
{
    mcslock_lock(&m, node);
}

EXPORT bool ext::mcslock::try_lock(mcs_queue_ent_t *node)
{
    return mcslock_try_lock(&m, node);
}

EXPORT void ext::mcslock::unlock(mcs_queue_ent_t *node)
{
    mcslock_unlock(&m, node);
}
