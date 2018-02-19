#include "iocp.h"

iocp_t::iocp_t(iocp_t::callback_t callback, uintptr_t arg)
    : callback(callback)
    , arg(arg)
    , done_count(0)
    , expect_count(0)
    , err(errno_t::OK)
{
    assert(callback);
}

void iocp_t::set_expect(unsigned expect)
{
    unique_lock<ticketlock> hold(lock);
    expect_count = expect;

    if (done_count == expect)
        invoke_once(hold);
}

void iocp_t::set_result(errno_t err_code)
{
    // Only write not-ok to err to avoid losing split command errors
    if (unlikely(err_code != errno_t::OK))
        err = err_code;
}

void iocp_t::invoke()
{
    unique_lock<ticketlock> hold(lock);
    if (expect_count && ++done_count >= expect_count)
        invoke_once(hold);
}

void iocp_t::invoke_once(unique_lock<ticketlock> &hold)
{
    if (callback != nullptr) {
        callback_t temp = callback;
        callback = nullptr;
        hold.unlock();
        temp(err, arg);
    }
}

errno_t blocking_iocp_t::wait()
{
    unique_lock<ticketlock> hold(lock);
    while (!done)
        done_cond.wait(hold);
    errno_t status = get_error();
    return status;
}
