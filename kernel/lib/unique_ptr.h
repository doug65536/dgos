#pragma once
#include "types.h"

template<typename T>
struct default_delete
{
    constexpr default_delete() = default;

    inline void operator()(T* ptr) const
    {
        delete ptr;
    }
};

template<typename T>
struct default_delete<T[]>
{
    constexpr default_delete() = default;

    inline void operator()(T* ptr) const
    {
        delete[] ptr;
    }
};

template<typename T,
         typename Tdeleter = default_delete<T>>
class unique_ptr
{
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = T const*;

    unique_ptr()
        : ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr&& rhs)
        : ptr(rhs.release())
    {
    }

    unique_ptr(unique_ptr const &) = delete;

    unique_ptr(pointer value)
        : ptr(value)
    {
    }

    ~unique_ptr()
    {
        if (ptr)
            ((Tdeleter()))(ptr);
    }

    operator pointer()
    {
        return ptr;
    }

    operator const_pointer() const
    {
        return ptr;
    }

    pointer operator->()
    {
        return ptr;
    }

    const_pointer operator->() const
    {
        return ptr;
    }

    pointer get()
    {
        return ptr;
    }

    const_pointer get() const
    {
        return ptr;
    }

    unique_ptr &operator=(pointer rhs)
    {
        if (ptr)
            ((Tdeleter()))(ptr);
        ptr = rhs;
        return *this;
    }

    Tdeleter get_deleter()
    {
        return Tdeleter();
    }

    pointer release()
    {
        pointer p = ptr;
        ptr = nullptr;
        return p;
    }

    void reset(pointer p = pointer())
    {
        pointer old = ptr;
        ptr = p;
        if (old)
            ((Tdeleter()))(old);
    }

private:
    pointer ptr;
};

template<typename T>
class free_deleter
{
public:
    constexpr free_deleter() = default;

    inline void operator()(T* ptr) const
    {
        if (ptr) {
            destruct(ptr);
            free(ptr);
        }
    }

private:
    void destruct(void *) const
    {
    }

    template<typename U>
    void destruct(U *ptr) const
    {
        ptr->~U();
    }
};

template<typename T>
using unique_ptr_free = unique_ptr<T, free_deleter<T>>;
