#pragma once
#include "type_traits.h"

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
