#pragma once
#ifndef _FUNCTIONAL_H_
#define _FUNCTIONAL_H_

#include "types.h"

__BEGIN_NAMESPACE_STD

template<typename>
class function;

template<typename>
struct equal_to;

template<typename>
struct less;

template<typename>
struct less_equal;

template<typename>
struct greater;

template<typename>
struct greater_equal;

template<typename>
class reference_wrapper;

__END_NAMESPACE_STD

#include "unique_ptr.h"
#include "utility.h"
#include "algorithm.h"
#include "type_traits.h"

__BEGIN_NAMESPACE_STD

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
        : impl(new (ext::nothrow) Callable<T>(move(callable)))
    {
    }

    template<typename _T>
    function(R (_T::*member)(_Args&&...), _T const* instance)
        : impl(new (ext::nothrow) Callable<_T>(member, instance))
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
        impl.reset(new (ext::nothrow) Callable<_C>(move(callable)));
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
        virtual ~CallableBase() = 0;
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
            return new (ext::nothrow) Callable(storage);
        }

        R invoke(_Args&& ...args) const override final
        {
            return storage(forward<_Args>(args)...);
        }

        T storage;
    };

    std::unique_ptr<CallableBase> impl;
};

template<typename R, typename... _Args>
function<R(_Args...)>::CallableBase::~CallableBase()
{
}

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
    using is_transparent = false_type;

    bool constexpr operator()(_T const& __lhs, _T const& __rhs) const
    {
        return __lhs < __rhs;
    }
};

template<>
struct less<void>
{
    using is_transparent = true_type;

    template<typename _T1, typename _T2>
    auto constexpr operator()(_T1 const& __lhs, _T2 const& __rhs) const
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

#endif
