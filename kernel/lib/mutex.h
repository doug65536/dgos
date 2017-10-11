#pragma once
#include "threadsync.h"

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

	mutex_t& native_handle()
	{
		return m;
	}

private:
	mutex_t m;
};

// Meets BasicLockable requirements
class spinlock {
public:
	typedef spinlock_t mutex_type;

	spinlock()
		: m(0)
	{
	}

	~spinlock()
	{
	}

	spinlock(spinlock const& r) = delete;

	void lock()
	{
		spinlock_lock_noirq(&m);
	}

	bool try_lock()
	{
		return spinlock_try_lock_noirq(&m);
	}

	void unlock()
	{
		spinlock_unlock_noirq(&m);
	}

	spinlock_t& native_handle()
	{
		return m;
	}

private:
	spinlock_t m;
};

struct defer_lock_t {
};

template<typename T>
class unique_lock
{
public:
    unique_lock(T& m)
        : m(m)
        , locked(false)
	{
		lock();
	}

	unique_lock(T& lock, defer_lock_t)
		: m(lock)
		, locked(false)
	{
	}

	~unique_lock()
	{
		unlock();
	}

	void lock()
	{
		assert(!locked);
		m.lock();
		locked = true;
	}

	void unlock()
	{
		if (locked) {
			locked = false;
			m.unlock();
		}
	}

	typename T::mutex_type& native_handle()
	{
		return m.native_handle();
	}

private:
	T& m;
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
		condvar_wait(&m, &lock.native_handle());
	}

	void wait(unique_lock<spinlock>& lock)
	{
		condvar_wait_spinlock(&m, &lock.native_handle());
	}

private:
	condition_var_t m;
};
