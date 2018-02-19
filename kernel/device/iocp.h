#pragma once
#include "errno.h"
#include "mutex.h"

// I/O completion
struct iocp_t {
    typedef void (*callback_t)(errno_t err, uintptr_t arg);

    iocp_t(callback_t callback, uintptr_t arg);

    // A single request can be split into multiple requests at
    // the device driver layer. This allows a single completion
    // to be shared by all of the split operations. The actual
    // callback will be invoked when invoke is called `expect`
    // times. Invoke will never be called if set_expect is never
    // called.
    void set_expect(unsigned expect);

    void set_result(errno_t err_code);

    void invoke();

    void operator()()
    {
        invoke();
    }

    errno_t get_error() const
    {
        return err;
    }

private:
    void invoke_once(unique_lock<ticketlock> &hold);

    callback_t callback;
    uintptr_t arg;
    unsigned done_count;
    unsigned expect_count;
    ticketlock lock;
    errno_t err;
};

class blocking_iocp_t : public iocp_t {
public:
    blocking_iocp_t()
        : iocp_t(&blocking_iocp_t::handler, uintptr_t(this))
        , done(false)
    {
    }

    static void handler(errno_t err, uintptr_t arg)
    {
        return ((blocking_iocp_t*)arg)->handler(err);
    }

    void handler(errno_t)
    {
        unique_lock<ticketlock> hold(lock);
        assert(!done);
        done = true;
        done_cond.notify_all();
        // Hold lock until after notify to ensure that the object
        // won't get destructed from under us
    }

    errno_t wait();

private:
    ticketlock lock;
    condition_variable done_cond;
    bool done;
};
