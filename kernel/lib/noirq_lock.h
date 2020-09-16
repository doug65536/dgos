#pragma once
#include "types.h"

__BEGIN_NAMESPACE_EXT
template<typename _L>
class unique_lock;
__END_NAMESPACE_EXT

__BEGIN_NAMESPACE_EXT
template<typename _L>
class KERNEL_API noirq_lock
{
public:
    using mutex_type = typename _L::mutex_type;

    noirq_lock()
        : inner_lock()
        , inner_hold(inner_lock, ext::defer_lock_t())
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
        inner_hold.lock();
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
        inner_hold.unlock();
        cpu_irq_toggle(irq_was_enabled);
    }

    void lock_noirq()
    {
        inner_hold.lock();
    }

    void unlock_noirq()
    {
        inner_hold.unlock();
    }

private:
    _L inner_lock;
    ext::unique_lock<_L> inner_hold;
    bool irq_was_enabled = false;
};
