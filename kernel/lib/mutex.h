#pragma once
#include "threadsync.h"
#include "utility.h"
#include "export.h"
#include "chrono.h"
#include "cpu/control_regs.h"

__BEGIN_NAMESPACE_EXT
class mutex;
class shared_mutex;
template<typename T> class unique_lock;
template<typename T> class shared_lock;

struct defer_lock_t {
};

class irqlock;
class ticketlock;
class spinlock;
class shared_spinlock;
class mcslock;

template<typename _L>
class base_lock {
public:
    _always_inline void lock_noirq()
    {
        static_cast<_L*>(this)->lock();
    }

    _always_inline void unlock_noirq()
    {
        static_cast<_L*>(this)->unlock();
    }
};

// Not a "real" lock, just saves and disables IRQs when locked,
// and restores saved IRQ mask when unlocked
class KERNEL_API irqlock {
public:
    using mutex_type = int;

    constexpr irqlock() {}
    ~irqlock();
    irqlock(irqlock const&) = delete;

    void lock();

    _always_inline bool try_lock()
    {
        // Failure is not possible
        lock();
        return true;
    }

    void unlock();

    // Do nothing
    void lock_noirq() {}
    void unlock_noirq() {}

    int& native_handle();

private:
    // 0 = undefined, -1 = irq was disabled, 1 = irq was enabled
    mutex_type saved_mask = 0;
};

// Meets BasicLockable requirements
class KERNEL_API spinlock : public base_lock<spinlock>
{
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

class KERNEL_API shared_spinlock : public base_lock<shared_spinlock> {
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
    bool try_lock_shared();
    void unlock_shared();
    void upgrade_lock();
    mutex_type& native_handle();

private:
    rwspinlock_t m;
};

// The size of this thing is two unsigned ints
class ticketlock : public base_lock<ticketlock> {
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
class KERNEL_API mcslock : public base_lock<mcslock> {
public:
    constexpr mcslock()
        : m(nullptr)
    {
    }

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

template<typename _L>
class KERNEL_API noirq_lock
{
public:
    using mutex_type = typename _L::mutex_type;

    constexpr noirq_lock()
        : inner_lock()
    {
    }

    noirq_lock(noirq_lock const&) = delete;
    noirq_lock(noirq_lock&&) = delete;
    noirq_lock &operator=(noirq_lock const&) = delete;

    mutex_type& native_handle()
    {
        return inner_lock.native_handle();
    }

    void lock()
    {
        irq_was_enabled = cpu_irq_save_disable();
        inner_lock.lock();
    }

    bool try_lock()
    {
        irq_was_enabled = cpu_irq_save_disable();
        if (inner_lock.try_lock())
            return true;
        cpu_irq_toggle(irq_was_enabled);
        return false;
    }

    void unlock()
    {
        inner_lock.unlock();
        cpu_irq_toggle(irq_was_enabled);
    }

    void lock_noirq() {
        inner_lock.lock();
    }

    void unlock_noirq() {
        inner_lock.unlock();
    }

private:
    _L inner_lock;
    bool irq_was_enabled = false;
};

template<>
class KERNEL_API noirq_lock<mcslock>
{
public:
    using mutex_type = typename mcslock::mutex_type;

    noirq_lock()
        : inner_lock()
    {
    }

    noirq_lock(noirq_lock const&) = delete;
    noirq_lock(noirq_lock&&) = delete;
    noirq_lock &operator=(noirq_lock const&) = delete;

    mutex_type& native_handle()
    {
        return inner_lock.native_handle();
    }

    void lock(mcs_queue_ent_t *node)
    {
        irq_was_enabled = cpu_irq_save_disable();
        inner_lock.lock(node);
    }

    bool try_lock(mcs_queue_ent_t *node)
    {
        irq_was_enabled = cpu_irq_save_disable();
        bool ok = inner_lock.try_lock(node);
        if (!ok)
            cpu_irq_toggle(irq_was_enabled);
        return ok;
    }

    void unlock(mcs_queue_ent_t *node)
    {
        inner_lock.unlock(node);
        cpu_irq_toggle(irq_was_enabled);
    }

private:
    mcslock inner_lock;
    bool irq_was_enabled = false;
};

using irq_mutex = noirq_lock<mutex>;
using irq_shared_mutex = noirq_lock<shared_mutex>;
using irq_mcslock = noirq_lock<mcslock>;
using irq_spinlock = noirq_lock<spinlock>;
using irq_ticketlock = noirq_lock<ticketlock>;

// Meets BasicLockable requirements
class KERNEL_API mutex : public base_lock<mutex> {
public:
    typedef mutex_t mutex_type;

    mutex();
    ~mutex();

    mutex(mutex const& r) = delete;

    void lock();
    bool try_lock();

    template<typename _Clock, typename _Duration>
    bool try_lock_until(chrono::time_point<_Clock, _Duration>
                        const& timeout_time);

    template<typename _Rep, typename _Period>
    bool try_lock_for(chrono::duration<_Rep, _Period> const& duration)
    {
        return try_lock_until(chrono::steady_clock::now() + duration);
    }

    void unlock();

    _always_inline mutex_type& native_handle()
    {
        return m;
    }

private:
    mutex_t m;
};

template<typename _Clock, typename _Duration>
bool mutex::try_lock_until(chrono::time_point<_Clock, _Duration>
                           const &timeout_time)
{
    chrono::steady_clock::time_point sys_timeout_time = timeout_time;

    uint64_t timeout_ns = sys_timeout_time.time_since_epoch().count();
    return mutex_try_lock_until(&m, timeout_ns);
}

// Meets SharedMutex requirements
class KERNEL_API shared_mutex : public ext::base_lock<shared_mutex> {
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

template<typename T>
class KERNEL_API unique_lock
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

    void lock() volatile noexcept
    {
        assert(!locked);
        m->lock();
        locked = true;
    }

    bool try_lock() noexcept
    {
        assert(!locked);
        locked = m->try_lock();
        return locked;
    }

    void unlock() noexcept
    {
        if (locked) {
            locked = false;
            m->unlock();
        }
    }

    void unlock() volatile noexcept
    {
        if (locked) {
            locked = false;
            m->unlock();
        }
    }

    void lock_noirq() noexcept
    {
        assert(!locked);
        m->lock_noirq();
        locked = true;
    }

    void unlock_noirq() noexcept
    {
        if (locked) {
            locked = false;
            m->unlock_noirq();
        }
    }

    void release() noexcept
    {
        locked = false;
        m = nullptr;
    }

    void swap(unique_lock& rhs) noexcept
    {
        ext::swap(rhs.m, m);
        ext::swap(rhs.locked, locked);
    }

    _always_inline typename T::mutex_type& native_handle() noexcept
    {
        return m->native_handle();
    }

    _always_inline bool is_locked() const noexcept
    {
        return locked;
    }

    _always_inline bool is_locked() volatile const noexcept
    {
        return locked;
    }

private:
    T* m;
    bool locked;
};

//template<typename _L>
//class EXPORT unique_lock<ext::noirq_lock<_L>>
//        : public ext::base_lock<unique_lock<ext::noirq_lock<_L>>>
//{
//public:
//    _hot
//    explicit unique_lock(ext::noirq_lock<_L>& attached_lock);

//    explicit unique_lock(ext::noirq_lock<_L>& lock, defer_lock_t) noexcept;

//    unique_lock(unique_lock const&) = delete;
//    unique_lock(unique_lock&&) = delete;
//    unique_lock& operator=(unique_lock) = delete;

//    _hot
//    ~unique_lock() noexcept;

//    _hot
//    void lock() noexcept;

//    _hot
//    void unlock() noexcept;

//    void release() noexcept;

//    void swap(unique_lock& rhs) noexcept;

//    _always_inline typename ext::noirq_lock<_L>::mutex_type&
//    native_handle() noexcept
//    {
//        return m->native_handle();
//    }

//    _always_inline bool is_locked() const noexcept
//    {
//        return locked;
//    }

//private:
//    ext::noirq_lock<_L> *m;
//    bool irq_was_enabled;
//    bool locked;
//};

//template<typename _T>
//EXPORT unique_lock<ext::noirq_lock<_T>>::unique_lock(
//        ext::noirq_lock<_T> &attached_lock)
//    : m(&attached_lock)
//    , locked(false)
//{
//    lock();
//}

//template<typename _T>
//EXPORT unique_lock<ext::noirq_lock<_T>>::unique_lock(
//        ext::noirq_lock<_T> &lock, ext::defer_lock_t) noexcept
//    : m(&lock)
//    , locked(false)
//{
//}

//template<typename _T>
//EXPORT unique_lock<ext::noirq_lock<_T>>::~unique_lock() noexcept
//{
//    unlock();
//}

//template<typename _T>
//EXPORT void unique_lock<ext::noirq_lock<_T>>::lock() noexcept
//{
//    assert(!locked);
//    irq_was_enabled = cpu_irq_save_disable();
//    m->lock();
//    locked = true;
//}

//template<typename _T>
//EXPORT void unique_lock<ext::noirq_lock<_T>>::unlock() noexcept
//{
//    if (locked) {
//        locked = false;
//        m->unlock();
//        cpu_irq_toggle(irq_was_enabled);
//    }
//}

//template<typename _T>
//EXPORT void unique_lock<ext::noirq_lock<_T>>::release() noexcept
//{
//    locked = false;
//    m = nullptr;
//}

//template<typename _T>
//EXPORT void unique_lock<ext::noirq_lock<_T>>::swap(
//        unique_lock<ext::noirq_lock<_T>> &rhs) noexcept
//{
//    ext::swap(rhs.m, m);
//    ext::swap(rhs.locked, locked);
//}

template<>
class KERNEL_API unique_lock<ext::mcslock>
        : public ext::base_lock<unique_lock<ext::mcslock>>
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
        ext::swap(rhs.m, m);
        ext::swap(rhs.locked, locked);
    }

private:
    T* m;
    bool locked;
};

enum class cv_status
{
    no_timeout,
    timeout
};

class KERNEL_API condition_variable
{
public:
    condition_variable();
    ~condition_variable();

    void notify_one();
    void notify_all();

    // Extension
    void notify_n(size_t n);

    template<typename L>
    void wait(unique_lock<L>& lock)
    {
        assert(lock.is_locked());
        condvar_wait_ex(&m, lock, uint64_t(-1));
        assert(lock.is_locked());
    }

    template<typename _Lock, typename _Clock, typename _Duration>
    cv_status wait_until(unique_lock<_Lock>& lock,
                         chrono::time_point<_Clock, _Duration> const&
                         timeout_time)
    {
        return wait_until(lock,
                          chrono::steady_clock::time_point(timeout_time).
                          time_since_epoch().count());
    }

    //
    // Underlying implementation uses monotonic time_ns time for timeout

    template<typename _Lock>
    cv_status wait_until(unique_lock<_Lock>& lock, uint64_t timeout_time)
    {
        assert(lock.is_locked());

        bool result = condvar_wait_ex(&m, lock, timeout_time);

        assert(lock.is_locked());

        return result
                ? ext::cv_status::no_timeout
                : ext::cv_status::timeout;
    }

    condition_var_t m;
};
__END_NAMESPACE_STD

__BEGIN_NAMESPACE_EXT
struct alignas(64) padded_mutex : public ext::mutex {
};

struct alignas(64) padded_condition_variable : public ext::condition_variable {
};

struct alignas(64) padded_ticketlock : public ticketlock {
};

struct alignas(64) padded_spinlock : public spinlock {
};

class alignas(64) padded_shared_mutex : public ext::shared_mutex {
};
__END_NAMESPACE_EXT

// Explicit instantiations

extern template class ext::unique_lock<ext::mutex>;
extern template class ext::unique_lock<ext::shared_mutex>;

extern template class ext::unique_lock<ext::shared_spinlock>;
extern template class ext::unique_lock<ext::ticketlock>;
extern template class ext::unique_lock<ext::spinlock>;
extern template class ext::unique_lock<ext::irqlock>;
extern template class ext::unique_lock<ext::mcslock>;

extern template class ext::unique_lock<ext::noirq_lock<ext::mutex>>;
extern template class ext::unique_lock<ext::noirq_lock<ext::shared_mutex>>;

extern template class ext::unique_lock<ext::noirq_lock<ext::shared_spinlock>>;
extern template class ext::unique_lock<ext::noirq_lock<ext::ticketlock>>;
extern template class ext::unique_lock<ext::noirq_lock<ext::spinlock>>;
extern template class ext::unique_lock<ext::noirq_lock<ext::irqlock>>;
extern template class ext::unique_lock<ext::noirq_lock<ext::mcslock>>;

