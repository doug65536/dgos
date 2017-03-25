#pragma once
#include "types.h"

template<typename T, size_t count>
class array
{
public:
    T &operator[](size_t index)
    {
        return data[index];
    }

    operator T*()
    {
        return data;
    }

    T const& operator[](size_t index) const
    {
        return data[index];
    }

    operator T const *() const
    {
        return data;
    }

    T *begin()
    {
        return data;
    }

    T *end()
    {
        return data + count;
    }

    T *begin() const
    {
        return data;
    }

    T *end() const
    {
        return data + count;
    }

    T *cbegin() const
    {
        return data;
    }

    T *cend() const
    {
        return data + count;
    }

private:
    T data[count];
};
