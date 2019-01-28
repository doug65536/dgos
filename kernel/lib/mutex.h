#pragma once
#include "threadsync.h"
#include "utility.h"

__BEGIN_NAMESPACE_STD

// Meets BasicLockable requirements
class mutex {
public:
    typedef mutex_t mutex_type;

    mutex()
    {
        mutex_init(&m);
    }

    ~mutex()
    {
        mutex_destroy(&m);
    }

    mutex(mutex const& r) = delete;

    void lock()
    {
        mutex_lock(&m);
    }

    bool try_lock()
    {
        return mutex_try_lock(&m);
    }

    void unlock()
    {
        mutex_unlock(&m);
    }

    mutex_type& native_handle()
    {
        return m;
    }

private:
    mutex_t m;
};

class alignas(64) padded_mutex : public mutex {
};

// Meets SharedMutex requirements
class shared_mutex {
public:
    typedef rwlock_t mutex_type;

    shared_mutex()
    {
        rwlock_init(&m);
    }

    ~shared_mutex()
    {
        rwlock_destroy(&m);
    }

    shared_mutex(mutex_type const& r) = delete;

    void lock()
    {
        rwlock_ex_lock(&m);
    }

    bool try_lock()
    {
        return rwlock_ex_try_lock(&m);
    }

    void unlock()
    {
        rwlock_ex_unlock(&m);
    }

    void lock_shared()
    {
        rwlock_sh_lock(&m);
    }

    void try_lock_shared()
    {
        rwlock_sh_try_lock(&m);
    }

    void unlock_shared()
    {
        rwlock_sh_unlock(&m);
    }

    void upgrade_lock()
    {
        rwlock_upgrade(&m);
    }

    mutex_type& native_handle()
    {
        return m;
    }

private:
    mutex_type m;
};

class alignas(64) padded_shared_mutex : public shared_mutex {
};

class shared_spinlock {
public:
    typedef rwspinlock_t mutex_type;

    constexpr shared_spinlock()
        : m(0)
    {
    }

    void lock()
    {
        rwspinlock_ex_lock(&m);
    }

    bool try_lock()
    {
        return rwspinlock_ex_try_lock(&m);
    }

    void unlock()
    {
        rwspinlock_ex_unlock(&m);
    }

    void lock_shared()
    {
        rwspinlock_sh_lock(&m);
    }

    void try_lock_shared()
    {
        rwspinlock_sh_try_lock(&m);
    }

    void unlock_shared()
    {
        rwspinlock_sh_unlock(&m);
    }

    void upgrade_lock()
    {
        rwspinlock_upgrade(&m);
    }

    mutex_type& native_handle()
    {
        return m;
    }

private:
    rwspinlock_t m;
};

// Meets BasicLockable requirements
class spinlock {
public:
    typedef spinlock_t mutex_type;

    constexpr spinlock()
        : m(0)
    {
    }

    ~spinlock()
    {
        assert(m == 0);
    }

    spinlock(spinlock const& r) = delete;

    void lock()
    {
        spinlock_lock(&m);
    }

    bool try_lock()
    {
        return spinlock_try_lock(&m);
    }

    void unlock()
    {
        spinlock_unlock(&m);
    }

    spinlock_t& native_handle()
    {
        return m;
    }

private:
    spinlock_t m;
};

struct alignas(64) padded_spinlock : public spinlock {
};

class ticketlock {
public:
    typedef ticketlock_t mutex_type;

    constexpr ticketlock()
        : m{}
    {
    }

    void lock()
    {
        ticketlock_lock(&m);
    }

    bool try_lock()
    {
        return ticketlock_try_lock(&m);
    }

    void unlock()
    {
        ticketlock_unlock(&m);
    }

    ticketlock_t& native_handle()
    {
        return m;
    }

private:
    ticketlock_t m;
};

struct alignas(64) padded_ticketlock : public ticketlock {
};

// Does not meet BasicLockable requirements, lock holder maintains node
class mcslock {
public:
    _hot
    constexpr mcslock()
        : m(nullptr)
    {
    }

    _hot
    ~mcslock()
    {
        assert(m == nullptr);
    }

    using mutex_type = mcs_queue_ent_t * volatile;

    _hot
    void lock(mcs_queue_ent_t *node)
    {
        mcslock_lock(&m, node);
    }

    bool try_lock(mcs_queue_ent_t *node)
    {
        return mcslock_try_lock(&m, node);
    }

    _hot
    void unlock(mcs_queue_ent_t *node)
    {
        mcslock_unlock(&m, node);
    }

    mcs_queue_ent_t * volatile &native_handle()
    {
        return m;
    }

private:
    mcs_queue_ent_t * volatile m;
};

struct defer_lock_t {
};

template<typename T>
class unique_lock
{
public:
    _hot
    explicit unique_lock(T& m) noexcept
        : m(&m)
        , locked(false)
    {
        lock();
    }

    explicit unique_lock(T& lock, defer_lock_t) noexcept
        : m(&lock)
        , locked(false)
    {
    }

    _hot
    ~unique_lock() noexcept
    {
        unlock();
    }

    void lock() noexcept
    {
        assert(!locked);
        m->lock();
        locked = true;
    }

    void unlock() noexcept
    {
        if (locked) {
            locked = false;
            m->unlock();
        }
    }

    void release() noexcept
    {
        locked = false;
        m = nullptr;
    }

    void swap(unique_lock& rhs) noexcept
    {
        std::swap(rhs.m, m);
        std::swap(rhs.locked, locked);
    }

    typename T::mutex_type& native_handle() noexcept
    {
        return m->native_handle();
    }

    bool is_locked() const noexcept
    {
        return locked;
    }

private:
    T* m;
    bool locked;
};

template<>
class unique_lock<mcslock>
{
public:
    _hot
    explicit unique_lock(mcslock& attached_lock)
        : m(&attached_lock)
        , locked(false)
    {
        lock();
    }

    explicit unique_lock(mcslock& lock, defer_lock_t) noexcept
        : m(&lock)
        , locked(false)
    {
    }

    unique_lock(unique_lock const&) = delete;
    unique_lock(unique_lock&&) = delete;
    unique_lock& operator=(unique_lock) = delete;

    _hot
    ~unique_lock() noexcept
    {
        unlock();
    }

    _hot
    void lock() noexcept
    {
        assert(!locked);
        m->lock(&node);
        locked = true;
    }

    _hot
    void unlock() noexcept
    {
        if (locked) {
            locked = false;
            m->unlock(&node);
        }
    }

    void release() noexcept
    {
        locked = false;
        m = nullptr;
    }

    void swap(unique_lock& rhs) noexcept
    {
        std::swap(rhs.m, m);
        std::swap(rhs.locked, locked);
    }

    typename mcslock::mutex_type& native_handle() noexcept
    {
        return m->native_handle();
    }

    mcs_queue_ent_t &wait_node()
    {
        return node;
    }

    bool is_locked() const noexcept
    {
        return locked;
    }

private:
    mcslock *m;
    mcs_queue_ent_t node;
    bool locked;
};

template<typename T>
class shared_lock
{
public:
    _hot
    shared_lock(T& m)
        : m(&m)
        , locked(false)
    {
        lock();
    }

    shared_lock(T& lock, defer_lock_t)
        : m(&lock)
        , locked(false)
    {
    }

    _hot
    ~shared_lock()
    {
        unlock();
    }

    void lock()
    {
        assert(!locked);
        m->lock_shared();
        locked = true;
    }

    void unlock()
    {
        if (locked) {
            locked = false;
            m->unlock_shared();
        }
    }

    typename T::mutex_type& native_handle()
    {
        return m->native_handle();
    }

    void release()
    {
        locked = false;
        m = nullptr;
    }

    void swap(unique_lock<T>& rhs)
    {
        std::swap(rhs.m, m);
        std::swap(rhs.locked, locked);
    }

private:
    T* m;
    bool locked;
};

class condition_variable
{
public:
    condition_variable()
    {
        condvar_init(&m);
    }

    ~condition_variable()
    {
        condvar_destroy(&m);
    }

    void notify_one()
    {
        condvar_wake_one(&m);
    }

    void notify_all()
    {
        condvar_wake_all(&m);
    }

    void wait(unique_lock<mutex>& lock)
    {
        assert(lock.is_locked());
        condvar_wait(&m, &lock.native_handle());
        assert(lock.is_locked());
    }

    void wait(unique_lock<spinlock>& lock)
    {
        assert(lock.is_locked());
        condvar_wait_spinlock(&m, &lock.native_handle());
        assert(lock.is_locked());
    }

    void wait(unique_lock<ticketlock>& lock)
    {
        assert(lock.is_locked());
        condvar_wait_ticketlock(&m, &lock.native_handle());
        assert(lock.is_locked());
    }

    void wait(unique_lock<mcslock>& lock)
    {
        assert(lock.is_locked());
        condvar_wait_mcslock(&m, &lock.native_handle(), &lock.wait_node());
        assert(lock.is_locked());
    }

private:
    condition_var_t m;
};

struct alignas(64) padded_condition_variable : public condition_variable {
};

__END_NAMESPACE_STD
