#pragma once

template<typename _T>
struct remove_reference
{
    typedef T type;
};

template<typename _T >
struct remove_reference<_T&>
{
    typedef T type;
};

template<typename _T>
struct remove_reference<_T&&>
{
    typedef T type;
};
