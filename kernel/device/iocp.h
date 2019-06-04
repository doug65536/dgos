#pragma once
#include "errno.h"
#include "mutex.h"
#include "cpu/atomic.h"
#include "chrono.h"

namespace dgos {

// I/O completion
template<typename T, typename S = T>
class basic_iocp_t
{
public:
    typedef void (*callback_t)(T const& err, uintptr_t arg);

    basic_iocp_t();
    basic_iocp_t(callback_t callback, uintptr_t arg);

    ~basic_iocp_t();

    // A single request can be split into multiple requests at
    // the device driver layer. This allows a single completion
    // to be shared by all of the split operations. The actual
    // callback will be invoked when invoke is called `expect`
    // times. Invoke will never be called if set_expect is never
    // called.
    void set_expect(unsigned expect);
    void set_result(T const& sub_result);
    void invoke();
    void operator()();
    T& get_result();
    operator bool() const;
    void reset(callback_t callback);
    void reset(callback_t callback, uintptr_t arg);

private:
    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;

    void invoke_once(scoped_lock &hold);

    callback_t callback;
    uintptr_t arg;
    unsigned volatile done_count;
    unsigned expect_count;
    int result_count;
    lock_type lock;
    T result;
};

template<typename T, typename S = T>
class basic_blocking_iocp_t : public basic_iocp_t<T, S>
{
public:
    basic_blocking_iocp_t()
        : basic_iocp_t<T, S>(&basic_blocking_iocp_t::handler, uintptr_t(this))
        , done(false)
    {
    }

    void reset();

    static void handler(T const& err, uintptr_t arg);

    void handler(T const&);

    T wait();

    template<typename _Clock, typename _Duration>
    bool wait_until(std::chrono::time_point<_Clock, _Duration>
                    const& timeout_time)
    {
        return wait_until(std::chrono::steady_clock::time_point(
                              timeout_time).time_since_epoch().count());
    }

    bool wait_until(uint64_t timeout_time);

    template<typename U>
    U wait_and_return();

private:
    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;

    lock_type lock;
    std::condition_variable done_cond;
    bool volatile done;
};

using err_sz_pair_t = std::pair<errno_t, size_t>;

template<typename T>
class __basic_iocp_error_success_t
{
public:
    static bool succeeded(errno_t const& status);

    static bool succeeded(err_sz_pair_t const& p);
};

template<typename T>
bool __basic_iocp_error_success_t<T>::succeeded(errno_t const &status)
{
    return status == errno_t::OK;
}

template<typename T>
bool __basic_iocp_error_success_t<T>::succeeded(err_sz_pair_t const &p)
{
    return p.first == errno_t::OK;
}

template<typename T, typename S>
basic_iocp_t<T, S>::basic_iocp_t()
    : callback(nullptr)
    , arg(0)
    , done_count(0)
    , expect_count(0)
    , result_count(0)
    , result{}
{
}

template<typename T, typename S>
basic_iocp_t<T, S>::basic_iocp_t(
        basic_iocp_t::callback_t callback, uintptr_t arg)
    : callback(callback)
    , arg(arg)
    , done_count(0)
    , expect_count(0)
    , result_count(0)
    , result()
{
    assert(callback);
}

template<typename T, typename S>
basic_iocp_t<T, S>::~basic_iocp_t()
{
    scoped_lock hold(lock);
    assert(done_count == expect_count);
}

template<typename T, typename S>
EXPORT void basic_iocp_t<T, S>::set_expect(unsigned expect)
{
    scoped_lock hold(lock);
    expect_count = expect;

    if (done_count >= expect)
        invoke_once(hold);
}

template<typename T, typename S>
void basic_iocp_t<T, S>::set_result(T const& sub_result)
{
    // Only write not-ok to err to avoid losing split command errors
    if (result_count++ == 0 || !S::succeeded(sub_result))
        result = sub_result;
}

template<typename T, typename S>
void basic_iocp_t<T, S>::invoke()
{
    scoped_lock hold(lock);
    if (++done_count >= expect_count && expect_count)
        invoke_once(hold);
}

template<typename T, typename S>
void basic_iocp_t<T, S>::operator()()
{
    invoke();
}

template<typename T, typename S>
T &basic_iocp_t<T, S>::get_result()
{
    return result;
}

template<typename T, typename S>
basic_iocp_t<T, S>::operator bool() const
{
    return S::succeeded(result);
}

template<typename T, typename S>
void basic_iocp_t<T, S>::reset(callback_t callback)
{
    atomic_st_rel(&this->callback, callback);
    atomic_st_rel(&done_count, 0);
    atomic_st_rel(&expect_count, 0);
    atomic_st_rel(&result_count, 0);
}

template<typename T, typename S>
void basic_iocp_t<T, S>::reset(basic_iocp_t::callback_t callback, uintptr_t arg)
{
    reset(callback);
    atomic_st_rel(&this->arg, arg);
}

template<typename T, typename S>
void basic_iocp_t<T, S>::invoke_once(scoped_lock &hold)
{
    if (callback != nullptr) {
        callback_t temp = callback;
        callback = nullptr;
        hold.unlock();
        temp(result, arg);
    }
}

template<typename T, typename S>
void basic_blocking_iocp_t<T, S>::reset()
{
    basic_iocp_t<T, S>::reset(
                &basic_blocking_iocp_t::handler, uintptr_t(this));
    done = false;
}

template<typename T, typename S>
void basic_blocking_iocp_t<T, S>::handler(const T &err, uintptr_t arg)
{
    return ((basic_blocking_iocp_t<T, S>*)arg)->handler(err);
}

template<typename T, typename S>
void basic_blocking_iocp_t<T, S>::handler(const T &)
{
    scoped_lock hold(lock);
    assert(!done);
    done = true;
    done_cond.notify_all();
    // Hold lock until after notify to ensure that the object
    // won't get destructed from under us
}

template<typename T, typename S>
T basic_blocking_iocp_t<T, S>::wait()
{
    scoped_lock hold(lock);
    while (!done)
        done_cond.wait(hold);
    T status = basic_iocp_t<T, S>::get_result();
    return status;
}

template<typename T, typename S>
bool basic_blocking_iocp_t<T, S>::wait_until(uint64_t timeout_time)
{
    auto timeout = std::chrono::time_point<std::chrono::steady_clock>(
                std::chrono::nanoseconds(timeout_time));

    scoped_lock hold(lock);
    while (!done) {
        if (done_cond.wait_until(hold, timeout) == std::cv_status::timeout)
            return false;
    }
    return true;
}

// Returns size on success, otherwise negated errno
template<typename T, typename S>
template<typename U>
U basic_blocking_iocp_t<T,S>::wait_and_return()
{
    auto result = wait();
    if (likely(result.first == errno_t::OK))
        return U(result.second);
    return -U(result.first);
}

__END_NAMESPACE

using iocp_t = dgos::basic_iocp_t<
    dgos::err_sz_pair_t,
    dgos::__basic_iocp_error_success_t<dgos::err_sz_pair_t>
>;

using blocking_iocp_t = dgos::basic_blocking_iocp_t<
    dgos::err_sz_pair_t,
    dgos::__basic_iocp_error_success_t<dgos::err_sz_pair_t>
>;

// Explicit instantiations

extern template struct std::pair<errno_t, size_t>;

extern template class dgos::__basic_iocp_error_success_t<
        std::pair<errno_t, size_t>>;

extern template class dgos::basic_iocp_t<
        std::pair<errno_t, size_t>,
        dgos::__basic_iocp_error_success_t<
            std::pair<errno_t, size_t>>>;

extern template class dgos::basic_blocking_iocp_t<
        std::pair<errno_t, size_t>,
        dgos::__basic_iocp_error_success_t<
            std::pair<errno_t, size_t>>>;
