#pragma once
#include "types.h"
#include "stdlib.h"

__BEGIN_NAMESPACE_STD

template<typename T>
struct default_delete
{
    constexpr default_delete() = default;

    _always_inline void operator()(T* __ptr) const
    {
        delete __ptr;
    }
};

template<typename T>
struct default_delete<T[]>
{
    constexpr default_delete() = default;

    template<typename U>
    _always_inline void operator()(U __ptr) const
    {
        delete[] __ptr;
    }
};

template<typename _T, typename Tdeleter = default_delete<_T>>
class unique_ptr
{
public:
    using value_type = _T;
    using pointer = _T*;
    using const_pointer = _T const*;

    unique_ptr()
        : __ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr&& __rhs)
        : __ptr(__rhs.release())
    {
    }

    unique_ptr(unique_ptr const &) = delete;

    unique_ptr(_T* __value)
        : __ptr(__value)
    {
    }

    ~unique_ptr()
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
    }

    operator _T*()
    {
        return __ptr;
    }

    operator _T const*() const
    {
        return __ptr;
    }

    _T* operator->()
    {
        return __ptr;
    }

    _T const* operator->() const
    {
        return __ptr;
    }

    _T* get()
    {
        return __ptr;
    }

    _T const* get() const
    {
        return __ptr;
    }

    unique_ptr &operator=(_T* __rhs)
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
        __ptr = __rhs;
        return *this;
    }

    Tdeleter get_deleter()
    {
        return Tdeleter();
    }

    _T* release()
    {
        _T* __p = __ptr;
        __ptr = nullptr;
        return __p;
    }

    bool reset(_T* p = pointer())
    {
        _T* __old = __ptr;
        __ptr = p;
        if (__old)
            ((Tdeleter()))(__old);

        return __ptr != nullptr;
    }

private:
    _T* __ptr;
};

template<typename _T, typename Tdeleter>
class unique_ptr<_T[], Tdeleter>
{
public:
    using value_type = _T;
    using pointer = _T*;
    using const_pointer = _T const*;

    unique_ptr()
        : __ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr&& rhs)
        : __ptr(rhs.release())
    {
    }

    unique_ptr(unique_ptr const &) = delete;

    unique_ptr(_T* __value)
        : __ptr(__value)
    {
    }

    ~unique_ptr()
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
    }

    operator _T*()
    {
        return __ptr;
    }

    operator _T const*() const
    {
        return __ptr;
    }

    _T& operator[](size_t i)
    {
        return __ptr[i];
    }

    _T const& operator[](size_t __i) const
    {
        return __ptr[__i];
    }

    _T* operator->()
    {
        return __ptr;
    }

    _T const* operator->() const
    {
        return __ptr;
    }

    _T* get()
    {
        return __ptr;
    }

    _T const* get() const
    {
        return __ptr;
    }

    unique_ptr &operator=(_T* __rhs)
    {
        if (__ptr)
            ((Tdeleter()))(__ptr);
        __ptr = __rhs;
        return *this;
    }

    Tdeleter get_deleter()
    {
        return Tdeleter();
    }

    _T* release()
    {
        _T* __p = __ptr;
        __ptr = nullptr;
        return __p;
    }

    void reset(_T* __p = pointer())
    {
        _T* __old = __ptr;
        __ptr = __p;
        if (__old)
            ((Tdeleter()))(__old);
    }

private:
    _T* __ptr;
};

__END_NAMESPACE_STD

__BEGIN_NAMESPACE_EXT

template<typename T>
class free_deleter
{
public:
    constexpr free_deleter() = default;

    _always_inline void operator()(T* __ptr) const
    {
        if (__ptr) {
            destruct(__ptr);
            free(__ptr);
        }
    }

private:
    void destruct(void *) const
    {
    }

    template<typename U>
    void destruct(U *__ptr) const
    {
        __ptr->~U();
    }
};

template<typename T>
using unique_ptr_free = unique_ptr<T, free_deleter<T>>;

__END_NAMESPACE
