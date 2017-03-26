#pragma once
#include "type_traits"

template<typename _T>
typename std::remove_reference<_T>::type&&
move(_T&& __t) noexcept
{
    return static_cast<typename remove_reference<_T>::type&&>(__t);
}

template<typename _T>
constexpr typename std::remove_reference<_T>::type&&
move(_T&& __t) noexcept
{
    return static_cast<typename remove_reference<_T>::type&&>(__t);
}

