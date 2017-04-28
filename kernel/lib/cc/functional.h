#pragma once
#include "unique_ptr.h"
#include "utility.h"
#include "algorithm.h"

template<typename>
class function;

template<typename R, typename... Args>
class function<R(Args...)>
{
public:
    typedef R result_type;

    function()
    {
    }

    template<typename T>
    function(T callable)
        : impl(new Callable<T>(move(callable)))
    {
    }

    function(function const& rhs)
        : impl(rhs.impl.get() ? rhs.impl->copy() : nullptr)
    {
    }

    R operator()(Args ...args)
    {
        return impl->invoke(args...);
    }

    template<typename T>
    function& operator=(T&& callable)
    {
        impl.reset(new Callable<T>(forward(callable)));
    }

    operator bool() const
    {
        return impl.get();
    }

    function& swap(function& rhs)
    {
        ::swap(impl, rhs.impl);
    }

private:
    struct CallableBase
    {
        virtual ~CallableBase() {}
        virtual R invoke(Args ...args) = 0;
        virtual CallableBase *copy() = 0;
    };

    template<typename T>
    struct Callable : public CallableBase
    {
        Callable(T callable)
            : storage(move(callable))
        {
        }

        CallableBase *copy()
        {
            return new Callable(storage);
        }

        R invoke(Args&& ...args) final
        {
            return (*impl)(forward<Args>(args)...);
        }

        T storage;
    };

    unique_ptr<CallableBase> impl;
};

template<typename _T>
struct equal_to
{
    bool operator()(_T const& __lhs, _T const& __rhs) const
    {
        return __lhs == __rhs;
    }
};

template<>
struct equal_to<void>
{
    template<typename _T1, typename _T2>
    bool operator()(_T1 const& __lhs, _T2 const& __rhs) const
    {
        return __lhs == __rhs;
    }
};
