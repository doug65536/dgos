#pragma once
#include "types.h"

template<typename T>
struct default_delete
{
    constexpr default_delete() = default;

    void operator()(T *ptr) const
    {
        delete ptr;
    }
};

template<typename T>
struct default_delete<T[]>
{
    constexpr default_delete() = default;

    void operator()(T* ptr) const
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
    using reference = T&;
    using const_reference = T const&;

    unique_ptr()
        : ptr(nullptr)
    {
    }

    unique_ptr(unique_ptr const &) = delete;

    unique_ptr(pointer value)
        : ptr(value)
    {
    }

    ~unique_ptr()
    {
    }

    reference operator[](size_t index)
    {
        return ptr[index];
    }

    const_reference operator[](size_t index) const
    {
        return ptr[index];
    }

    operator pointer()
    {
        return ptr;
    }

    operator const_pointer() const
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
            deleter(ptr);
        ptr = rhs;
        return *this;
    }

    Tdeleter get_deleter()
    {
        return deleter;
    }

private:
    Tdeleter deleter;
    pointer ptr;
};
