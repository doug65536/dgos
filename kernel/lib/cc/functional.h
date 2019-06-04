#pragma once
#include "unique_ptr.h"
#include "utility.h"
#include "algorithm.h"

__BEGIN_NAMESPACE_STD

template<typename>
class function;

template<typename R, typename... _Args>
class function<R(_Args...)>
{
public:
    using result_type = R;

    function()
    {
    }

    template<typename T>
    function(T callable)
        : impl(new (std::nothrow) Callable<T>(move(callable)))
    {
    }

    template<typename _T>
    function(R (_T::*member)(_Args&&...), _T const* instance)
        : impl(new (std::nothrow) Callable<_T>(member, instance))
    {
    }

    function(function const& rhs)
        : impl(rhs.impl.get() ? rhs.impl->copy() : nullptr)
    {
    }

    template<typename ..._A>
    R operator()(_A&& ...args) const
    {
        return impl->invoke(forward<_A>(args)...);
    }

    template<typename _C>
    function& operator=(_C callable)
    {
        impl.reset(new (std::nothrow) Callable<_C>(move(callable)));
        return *this;
    }

    operator bool() const
    {
        return impl.get();
    }

    function& swap(function& rhs)
    {
        std::swap(impl, rhs.impl);
    }

private:
    struct CallableBase
    {
        virtual ~CallableBase() {}
        virtual R invoke(_Args&& ...args) const = 0;
        virtual CallableBase *copy() const = 0;
    };

    template<typename T>
    struct Callable : public CallableBase
    {
        Callable(T callable)
            : storage(move(callable))
        {
        }

        CallableBase *copy() const override final
        {
            return new (std::nothrow) Callable(storage);
        }

        R invoke(_Args&& ...args) const override final
        {
            return storage(forward<_Args>(args)...);
        }

        T storage;
    };

    std::unique_ptr<CallableBase> impl;
};

template<typename _T>
struct equal_to
{
    using result_type = bool;
    using first_argument = _T;
    using second_argument = _T;

    bool operator()(_T const& __lhs, _T const& __rhs) const
    {
        return __lhs == __rhs;
    }
};

template<>
struct equal_to<void>
{
    using result_type = bool;

    template<typename _T1, typename _T2>
    bool operator()(_T1 const& __lhs, _T2 const& __rhs) const
    {
        return __lhs == __rhs;
    }
};

template<typename _T>
struct less
{
    using result_type = bool;
    using first_argument = _T;
    using second_argument = _T;

    bool constexpr operator()(_T const& __lhs, _T const& __rhs) const
    {
        return __lhs < __rhs;
    }
};

template<>
struct less<void>
{
    using result_type = bool;

    template<typename _T1, typename _T2>
    bool constexpr operator()(_T1 const& __lhs, _T2 const& __rhs) const
    {
        return __lhs < __rhs;
    }
};

template<typename _T>
struct less_equal
{
    using result_type = bool;
    using first_argument = _T;
    using second_argument = _T;

    bool constexpr operator()(_T const& __lhs, _T const& __rhs) const
    {
        return __lhs <= __rhs;
    }
};

template<>
struct less_equal<void>
{
    using result_type = bool;

    template<typename _T1, typename _T2>
    bool constexpr operator()(_T1 const& __lhs, _T2 const& __rhs) const
    {
        return __lhs <= __rhs;
    }
};

template<typename _T>
struct greater
{
    using result_type = bool;
    using first_argument = _T;
    using second_argument = _T;

    bool constexpr operator()(_T const& __lhs, _T const& __rhs) const
    {
        return __lhs > __rhs;
    }
};

template<>
struct greater<void>
{
    using result_type = bool;

    template<typename _T1, typename _T2>
    bool constexpr operator()(_T1 const& __lhs, _T2 const& __rhs) const
    {
        return __lhs > __rhs;
    }
};

template<typename _T>
struct greater_equal
{
    using result_type = bool;
    using first_argument = _T;
    using second_argument = _T;

    bool constexpr operator()(_T const& __lhs, _T const& __rhs) const
    {
        return __lhs >= __rhs;
    }
};

template<>
struct greater_equal<void>
{
    using result_type = bool;

    template<typename _T1, typename _T2>
    bool constexpr operator()(_T1 const& __lhs, _T2 const& __rhs) const
    {
        return __lhs >= __rhs;
    }
};

__END_NAMESPACE_STD
