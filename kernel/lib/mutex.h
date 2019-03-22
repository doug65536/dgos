#pragma once
#include "threadsync.h"
#include "utility.h"
#include "export.h"

__BEGIN_NAMESPACE_STD
class mutex;
class shared_mutex;
template<typename T> class unique_lock;
template<typename T> class shared_lock;
__END_NAMESPACE_STD

__BEGIN_NAMESPACE_EXT
class ticketlock;
class spinlock;
class shared_spinlock;
class mcslock;
__END_NAMESPACE_EXT

__BEGIN_NAMESPACE_EXT
// Meets BasicLockable requirements
class spinlock {
public:
    typedef spinlock_t mutex_type;

    constexpr spinlock()
        : m(0)
    {
    }

    ~spinlock();

    spinlock(spinlock const& r) = delete;

    void lock();
    bool try_lock();
    void unlock();

    spinlock_t& native_handle();

private:
    spinlock_t m;
};

class shared_spinlock {
public:
    typedef rwspinlock_t mutex_type;

    constexpr shared_spinlock()
        : m(0)
    {
    }

    void lock();
    bool try_lock();
    void unlock();
    void lock_shared();
    void try_lock_shared();
    void unlock_shared();
    void upgrade_lock();
    mutex_type& native_handle();

private:
    rwspinlock_t m;
};
__END_NAMESPACE_STD

__BEGIN_NAMESPACE_EXT
// The size of this thing is two unsigned ints
class ticketlock {
public:
    typedef ticketlock_t mutex_type;

    constexpr ticketlock()
        : m{}
    {
    }

    void lock();

    bool try_lock();

    void unlock();

    ticketlock_t& native_handle();

private:
    ticketlock_t m;
};

// Does not meet BasicLockable requirements, lock holder maintains node
// The size of this thing is the size of one pointer
class mcslock {
public:
    mcslock();
    ~mcslock();

    using mutex_type = mcs_queue_ent_t * volatile;

    void lock(mcs_queue_ent_t *node);
    bool try_lock(mcs_queue_ent_t *node);
    void unlock(mcs_queue_ent_t *node);

    _always_inline mcs_queue_ent_t * volatile &native_handle()
    {
        return m;
    }

private:
    mcs_queue_ent_t * volatile m;
};
__END_NAMESPACE_EXT

__BEGIN_NAMESPACE_STD

// Meets BasicLockable requirements
class mutex {
public:
    typedef mutex_t mutex_type;

    mutex();
    ~mutex();

    mutex(mutex const& r) = delete;

    void lock();
    bool try_lock();
    void unlock();

    _always_inline mutex_type& native_handle()
    {
        return m;
    }

private:
    mutex_t m;
};

// Meets SharedMutex requirements
class shared_mutex {
public:
    typedef rwlock_t mutex_type;

    shared_mutex();
    ~shared_mutex();

    shared_mutex(mutex_type const& r) = delete;

    void lock();
    bool try_lock();
    void unlock();
    void lock_shared();
    void try_lock_shared();
    void unlock_shared();
    void upgrade_lock();

    _always_inline mutex_type& native_handle()
    {
        return m;
    }

private:
    mutex_type m;
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

    _always_inline typename T::mutex_type& native_handle() noexcept
    {
        return m->native_handle();
    }

    _always_inline bool is_locked() const noexcept
    {
        return locked;
    }

private:
    T* m;
    bool locked;
};

template<>
class unique_lock<ext::mcslock>
{
public:
    _hot
    explicit unique_lock(ext::mcslock& attached_lock);

    explicit unique_lock(ext::mcslock& lock, defer_lock_t) noexcept;

    unique_lock(unique_lock const&) = delete;
    unique_lock(unique_lock&&) = delete;
    unique_lock& operator=(unique_lock) = delete;

    _hot
    ~unique_lock() noexcept;

    _hot
    void lock() noexcept;

    _hot
    void unlock() noexcept;

    void release() noexcept;

    void swap(unique_lock& rhs) noexcept;

    _always_inline typename ext::mcslock::mutex_type& native_handle() noexcept
    {
        return m->native_handle();
    }

    _always_inline mcs_queue_ent_t &wait_node()
    {
        return node;
    }

    _always_inline bool is_locked() const noexcept
    {
        return locked;
    }

private:
    ext::mcslock *m;
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
    condition_variable();
    ~condition_variable();

    void notify_one();
    void notify_all();
    void wait(unique_lock<mutex>& lock);
    void wait(unique_lock<ext::spinlock>& lock);
    void wait(unique_lock<ext::ticketlock>& lock);
    void wait(unique_lock<ext::mcslock>& lock);

private:
    condition_var_t m;
};

__END_NAMESPACE_STD

__BEGIN_NAMESPACE_EXT
struct alignas(64) padded_mutex : public std::mutex {
};

struct alignas(64) padded_condition_variable : public std::condition_variable {
};

struct alignas(64) padded_ticketlock : public ticketlock {
};

struct alignas(64) padded_spinlock : public spinlock {
};

class alignas(64) padded_shared_mutex : public std::shared_mutex {
};
__END_NAMESPACE_EXT

// Explicit instantiations

extern template class std::unique_lock<std::mutex>;
extern template class std::unique_lock<std::shared_mutex>;

extern template class std::unique_lock<ext::shared_spinlock>;
extern template class std::unique_lock<ext::ticketlock>;
extern template class std::unique_lock<ext::spinlock>;
extern template class std::unique_lock<ext::mcslock>;

