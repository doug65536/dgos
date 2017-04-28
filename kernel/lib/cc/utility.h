#pragma once
#include "type_traits.h"
#include "initializer_list.h"
#include "string.h"

template<typename _T>
__always_inline
constexpr typename remove_reference<_T>::type&&
forward(_T& __t) noexcept
{
    return static_cast<_T&&>(__t);
}

template<typename _T>
__always_inline
constexpr typename remove_reference<_T>::type&&
forward(_T&& __t) noexcept
{
    static_assert(!is_lvalue_reference<_T>::value, "lvalue");
    return static_cast<_T&&>(__t);
}

template<typename _T>
__always_inline
constexpr typename remove_reference<_T>::type&&
move(_T&& __t) noexcept
{
    return static_cast<typename remove_reference<_T>::type&&>(__t);
}

template<typename _T>
void swap(_T& lhs, _T& rhs)
{
    _T tmp(move(lhs));
    lhs = move(rhs);
    rhs = move(tmp);
}

template<typename _T1, typename _T2>
struct pair
{
    typedef _T1 first_type;
    typedef _T2 second_type;

    constexpr pair()
        : first{}
        , second{}
    {
    }

    constexpr pair(first_type __first_value, second_type __second_value)
        : first(__first_value)
        , second(__second_value)
    {
    }

    first_type first;
    second_type second;
};

template<typename _T>
constexpr _T min(_T const& __lhs, _T const& __rhs)
{
    return __lhs <= __rhs ? __lhs : __rhs;
}

template<typename _T>
constexpr _T max(_T const& __lhs, _T const& __rhs)
{
    return __rhs <= __lhs ? __lhs : __rhs;
}

template<typename _T, typename _Compare>
constexpr _T min(_T const& __lhs, _T const& __rhs, _Compare __cmp)
{
    return __cmp(__lhs, __rhs) <= 0 ? __lhs : __rhs;
}

template<typename _T, typename _Compare>
constexpr _T max(_T const& __lhs, _T const& __rhs, _Compare __cmp)
{
    return __cmp(__rhs, __lhs) <= 0 ? __lhs : __rhs;
}

template<typename _T, typename _Compare>
constexpr _T min(initializer_list<_T> __list)
{
    return *min_element(__list.begin(), __list.end());
}

template<typename _T, typename _Compare>
constexpr _T max(initializer_list<_T> __list)
{
    return *max_element(__list.begin(), __list.end());
}

template<typename _T, typename _Compare>
constexpr _T min(initializer_list<_T> __list, _Compare __cmp)
{
    return *min_element(__list.begin(), __list.end(), __cmp);
}

template<typename _T, typename _Compare>
constexpr _T max(initializer_list<_T> __list, _Compare __cmp)
{
    return *max_element(__list.begin(), __list.end(), __cmp);
}

template<typename> struct hash;
template<> struct hash<bool>
{
    size_t operator()(bool __k) const { return size_t(__k); }
};

template<> struct hash<char>
{
    size_t operator()(char __k) const { return size_t(__k); }
};

template<> struct hash<signed char>
{
    size_t operator()(signed char __k) const { return size_t(__k); }
};

template<> struct hash<unsigned char>
{
    size_t operator()(unsigned char __k) const { return size_t(__k); }
};

template<> struct hash<char16_t>
{
    size_t operator()(char16_t __k) const { return size_t(__k); }
};

template<> struct hash<char32_t>
{
    size_t operator()(char32_t __k) const { return size_t(__k); }
};

template<> struct hash<wchar_t>
{
    size_t operator()(wchar_t __k) const { return size_t(__k); }
};

template<> struct hash<short>
{
    size_t operator()(short __k) const { return size_t(__k); }
};

template<> struct hash<unsigned short>
{
    size_t operator()(unsigned short __k) const { return size_t(__k); }
};

template<> struct hash<int>
{
    size_t operator()(int __k) const { return size_t(__k); }
};

template<> struct hash<unsigned>
{
    size_t operator()(unsigned __k) const { return size_t(__k); }
};

template<> struct hash<long>
{
    size_t operator()(long __k) const { return size_t(__k); }
};

template<> struct hash<long long>
{
    size_t operator()(long long __k) const { return size_t(__k); }
};

template<> struct hash<unsigned long>
{
    size_t operator()(unsigned long __k) const { return size_t(__k); }
};

template<> struct hash<unsigned long long>
{
    size_t operator()(unsigned long long __k) const { return size_t(__k); }
};

template<> struct hash<float>
{
    size_t operator()(float __k) const
    {
        size_t __h = 0;
        memcpy(&__h, &__k, min(sizeof(__k), sizeof(__h)));
        return __h;
    }
};

template<> struct hash<double>
{
    size_t operator()(double __k) const
    {
        size_t __h = 0;
        memcpy(&__h, &__k, min(sizeof(__k), sizeof(__h)));
        return __h;
    }
};

template<> struct hash<long double>
{
    size_t operator()(double __k) const
    {
        size_t __h = 0;
        memcpy(&__h, &__k, min(sizeof(__k), sizeof(__h)));
        return __h;
    }
};

template<typename _T> struct hash<_T*>
{
    size_t operator()(_T* __k) const { return size_t(__k); }
};

template<typename _K, typename _V> struct hash<pair<_K, _V>>
{
    size_t operator()(pair<_K, _V> const& __k) const
    {
        size_t __kh = hash<_K>()(__k.first);
        size_t __vh = hash<_V>()(__k.second);
        return __kh ^ ((__vh << (sizeof(size_t)/2)) |
                       (__vh >> (sizeof(size_t)/2)));
    }
};

