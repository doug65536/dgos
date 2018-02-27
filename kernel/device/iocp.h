#pragma once
#include "errno.h"
#include "mutex.h"

// I/O completion
template<typename T, typename S = T>
struct basic_iocp_t {
    typedef void (*callback_t)(T const& err, uintptr_t arg);

    basic_iocp_t(callback_t callback, uintptr_t arg);

    // A single request can be split into multiple requests at
    // the device driver layer. This allows a single completion
    // to be shared by all of the split operations. The actual
    // callback will be invoked when invoke is called `expect`
    // times. Invoke will never be called if set_expect is never
    // called.
    void set_expect(unsigned expect);

    void set_result(T const& sub_result);

    void invoke();

    void operator()()
    {
        invoke();
    }

    T& get_result()
    {
        return result;
    }

    operator bool() const
    {
        return S::succeeded(result);
    }

    void reset(callback_t callback);

private:
    void invoke_once(unique_lock<ticketlock> &hold);

    callback_t callback;
    uintptr_t arg;
    unsigned done_count;
    unsigned expect_count;
    int result_count;
    ticketlock lock;
    T result;
};

template<typename T, typename S = T>
class basic_blocking_iocp_t : public basic_iocp_t<T, S> {
public:
    basic_blocking_iocp_t()
        : basic_iocp_t<T, S>(&basic_blocking_iocp_t::handler, uintptr_t(this))
        , done(false)
    {
    }

    static void handler(T const& err, uintptr_t arg)
    {
        return ((basic_blocking_iocp_t<T, S>*)arg)->handler(err);
    }

    void handler(T const&)
    {
        unique_lock<ticketlock> hold(lock);
        assert(!done);
        done = true;
        done_cond.notify_all();
        // Hold lock until after notify to ensure that the object
        // won't get destructed from under us
    }

    T wait();

private:
    ticketlock lock;
    condition_variable done_cond;
    bool done;
};

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
void basic_iocp_t<T, S>::set_expect(unsigned expect)
{
    unique_lock<ticketlock> hold(lock);
    expect_count = expect;

    if (done_count == expect)
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
    unique_lock<ticketlock> hold(lock);
    if (expect_count && ++done_count >= expect_count)
        invoke_once(hold);
}

template<typename T, typename S>
void basic_iocp_t<T, S>::reset(callback_t callback)
{
    this->callback = callback;
    done_count = 0;
    expect_count = 0;
    result_count = 0;
}

template<typename T, typename S>
void basic_iocp_t<T, S>::invoke_once(unique_lock<ticketlock> &hold)
{
    if (callback != nullptr) {
        callback_t temp = callback;
        callback = nullptr;
        hold.unlock();
        temp(result, arg);
    }
}

template<typename T, typename S>
T basic_blocking_iocp_t<T, S>::wait()
{
    unique_lock<ticketlock> hold(lock);
    while (!done)
        done_cond.wait(hold);
    T status = basic_iocp_t<T, S>::get_result();
    return status;
}

template<typename T>
struct __basic_iocp_error_success_t {
    static constexpr bool succeeded(errno_t const& status)
    {
        return status == errno_t::OK;
    }
};

using iocp_t = basic_iocp_t<errno_t, __basic_iocp_error_success_t<errno_t>>;
using blocking_iocp_t = basic_blocking_iocp_t<errno_t,
    __basic_iocp_error_success_t<errno_t>>;
