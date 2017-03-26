#pragma once
#include "types.h"

template<typename _T>
class initializer_list
{
public:
    typedef _T value_type;
    typedef size_t size_type;
    typedef _T const& reference;
    typedef _T const& const_reference;
    typedef _T const* iterator;
    typedef _T const* const_iterator;

private:
    iterator __array;
    size_type __len;

    // The compiler can call a private constructor.
    constexpr initializer_list(const_iterator __a, size_type __l)
        : __array(__a)
        , __len(__l)
    {
    }

public:
    constexpr initializer_list() noexcept
        : __array(nullptr)
        , __len(0) { }

    constexpr size_type
    size() const noexcept
    {
        return __len;
    }

    constexpr const_iterator
    begin() const noexcept
    {
        return __array;
    }

    constexpr const_iterator
    end() const noexcept
    {
        return begin() + size();
    }
};

template<typename _T>
constexpr const _T*
begin(initializer_list<_T> __ils) noexcept
{
    return __ils.begin();
}

template<typename _T>
constexpr const _T*
end(initializer_list<_T> __ils) noexcept
{
    return __ils.end();
}
